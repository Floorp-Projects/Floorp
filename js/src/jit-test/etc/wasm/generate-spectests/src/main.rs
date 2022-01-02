use std::env;
use std::ffi::OsStr;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{bail, Result};
use regex::{RegexSet, RegexSetBuilder};
use serde_derive::{Deserialize, Serialize};
use toml;
use wast2js;

use log::{debug, info, warn};

// Data structures

#[derive(Debug, Default, Serialize, Deserialize)]
struct Config {
    #[serde(default)]
    harness_directive: Option<String>,
    #[serde(default)]
    directive: Option<String>,
    #[serde(default)]
    included_tests: Vec<String>,
    #[serde(default)]
    excluded_tests: Vec<String>,
    repos: Vec<Repo>,
}

impl Config {
    fn find_repo_mut(&mut self, name: &str) -> Option<&mut Repo> {
        self.repos.iter_mut().find(|x| &x.name == name)
    }
}

#[derive(Debug, Default, Serialize, Deserialize)]
struct Repo {
    name: String,
    url: String,
    #[serde(default)]
    branch: Option<String>,
    #[serde(default)]
    parent: Option<String>,
    #[serde(default)]
    directive: Option<String>,
    #[serde(default)]
    included_tests: Vec<String>,
    #[serde(default)]
    excluded_tests: Vec<String>,
    #[serde(default)]
    skip_wast: bool,
    #[serde(default)]
    skip_js: bool,
}

#[derive(Debug, Default, Serialize, Deserialize)]
struct Lock {
    repos: Vec<LockRepo>,
}

impl Lock {
    fn find_commit(&self, name: &str) -> Option<&str> {
        self.repos
            .iter()
            .find(|x| &x.name == name)
            .map(|x| x.commit.as_ref())
    }

    fn set_commit(&mut self, name: &str, commit: &str) {
        if let Some(lock) = self.repos.iter_mut().find(|x| &x.name == name) {
            lock.commit = commit.to_owned();
        } else {
            self.repos.push(LockRepo {
                name: name.to_owned(),
                commit: commit.to_owned(),
            });
        }
    }
}

#[derive(Debug, Default, Serialize, Deserialize)]
struct LockRepo {
    name: String,
    commit: String,
}

#[derive(Debug)]
enum Merge {
    Standalone,
    Merged,
    Conflicted,
}

#[derive(Debug)]
struct Status {
    commit_base_hash: String,
    commit_final_message: String,
    merged: Merge,
    built: bool,
}

// Roll-your-own CLI utilities

fn run(name: &str, args: &[&str]) -> Result<String> {
    debug!("{} {:?}", name, args);
    let output = Command::new(name).args(args).output()?;
    let stdout = String::from_utf8(output.stdout)?.trim().to_owned();
    let stderr = String::from_utf8(output.stderr)?.trim().to_owned();
    if !stdout.is_empty() {
        debug!("{}", stdout);
    }
    if !stderr.is_empty() {
        debug!("{}", stderr);
    }

    if output.status.success() {
        Ok(stdout)
    } else {
        bail!("{}: {}\n{}", name.to_owned(), stdout, stderr)
    }
}

fn change_dir(dir: &str) -> impl Drop {
    #[must_use]
    struct Reset {
        previous: PathBuf,
    }
    impl Drop for Reset {
        fn drop(&mut self) {
            debug!("cd {}", self.previous.display());
            env::set_current_dir(&self.previous).unwrap()
        }
    }

    let previous = Reset {
        previous: env::current_dir().unwrap(),
    };
    debug!("cd {}", dir);
    env::set_current_dir(dir).unwrap();
    previous
}

fn find(dir: &str) -> Vec<PathBuf> {
    let mut paths = Vec::new();

    fn find(dir: &str, paths: &mut Vec<PathBuf>) {
        for entry in fs::read_dir(dir).unwrap().map(|x| x.unwrap()) {
            let path = entry.path();

            if entry.file_type().unwrap().is_dir() {
                find(path.to_str().unwrap(), paths);
            } else {
                paths.push(path);
            }
        }
    }

    find(dir, &mut paths);
    paths
}

fn write_string<P: AsRef<Path>>(path: P, text: &str) -> Result<()> {
    let path = path.as_ref();
    if let Some(dir) = path.parent() {
        let _ = fs::create_dir_all(dir);
    }
    fs::write(path, text.as_bytes())?;
    Ok(())
}

// The main script

fn main() {
    env_logger::init();

    // Load the config
    let mut config: Config =
        toml::from_str(&fs::read_to_string("config.toml").expect("failed to read config.toml"))
            .expect("invalid config.toml");

    // Load the lock file, or default to no pinned commits
    let mut lock: Lock = if Path::new("config-lock.toml").exists() {
        toml::from_str(
            &fs::read_to_string("config-lock.toml").expect("failed to read config-lock.toml"),
        )
        .expect("invalid config-lock.toml")
    } else {
        Lock::default()
    };

    // Clean old tests and initialize the repo if it doesn't exist
    let specs_dir = "specs/";
    clean_and_init_dirs(specs_dir);

    // Generate the tests
    let mut successes = Vec::new();
    let mut failures = Vec::new();
    {
        // Change to the `specs/` dir where all the work happens
        let _cd = change_dir(specs_dir);
        for repo in &config.repos {
            info!("Processing {:#?}", repo);

            match build_repo(repo, &config, &lock) {
                Ok(status) => successes.push((repo.name.clone(), status)),
                Err(err) => failures.push((repo.name.clone(), err)),
            };
        }
    }

    // Abort if we had a failure
    if !failures.is_empty() {
        warn!("Failed.");
        for (name, err) in &failures {
            warn!("{}: (failure) {:?}", name, err);
        }
        std::process::exit(1);
    }

    // Display successful results
    info!("Done.");
    for (name, status) in &successes {
        let repo = config.find_repo_mut(&name).unwrap();
        lock.set_commit(&name, &status.commit_base_hash);

        info!(
            "{}: ({} {}) {}",
            repo.name,
            match status.merged {
                Merge::Standalone => "standalone",
                Merge::Merged => "merged",
                Merge::Conflicted => "conflicted",
            },
            if status.built { "building" } else { "broken" },
            status.commit_final_message.trim_end()
        );
    }

    // Commit the new lock file
    write_string("config-lock.toml", &toml::to_string_pretty(&lock).unwrap()).unwrap();
}

fn clean_and_init_dirs(specs_dir: &str) {
    if !Path::new(specs_dir).exists() {
        fs::create_dir(specs_dir).unwrap();
        run("git", &["-C", specs_dir, "init"]).unwrap();
    }

    let _ = fs::remove_dir_all("./tests");
}

fn build_repo(repo: &Repo, config: &Config, lock: &Lock) -> Result<Status> {
    let remote_name = &repo.name;
    let remote_url = &repo.url;
    let remote_branch = repo.branch.as_ref().map(|x| x.as_str()).unwrap_or("master");
    let branch_upstream = format!("{}/{}", repo.name, remote_branch);
    let branch_base = repo.name.clone();

    // Initialize our remote and branches if they don't exist
    let remotes = run("git", &["remote"])?;
    if !remotes.lines().any(|x| x == repo.name) {
        run("git", &["remote", "add", remote_name, &remote_url])?;
        run("git", &["fetch", remote_name])?;
        run("git", &["branch", &branch_base, &branch_upstream])?;
    }

    // Set the upstream to the correct branch
    run(
        "git",
        &[
            "branch",
            &branch_base,
            "--set-upstream-to",
            &branch_upstream,
        ],
    )?;

    // Fetch the latest changes for this repo
    run("git", &["fetch", remote_name])?;

    // Checkout the pinned commit, if any, and get the absolute commit hash
    let base_treeish = lock.find_commit(&repo.name).unwrap_or(&branch_upstream);
    run("git", &["checkout", &branch_base])?;
    run("git", &["reset", base_treeish, "--hard"])?;
    let commit_base_hash = run("git", &["log", "--pretty=%h", "-n", "1"])?
        .trim()
        .to_owned();

    // Try to merge with parent repo, if specified
    let merged = try_merge_parent(repo, &commit_base_hash)?;

    // Try to build the test suite on this commit. This may fail due to merging
    // with a parent repo, in which case we will try again in an unmerged state.
    let mut built = false;
    match try_build_tests() {
        Ok(()) => built = true,
        Err(err) => warn!("Failed to build tests: {:?}", err),
    };
    // if try_build_tests().is_err() {
    //     if repo.parent.is_some() {
    //         warn!(
    //             "Failed to build interpreter. Retrying on unmerged commit ({})",
    //             &commit_base_hash
    //         );
    //         run("git", &["reset", &commit_base_hash, "--hard"])?;
    //         built = try_build_tests().is_ok();
    //     } else {
    //         built = false;
    //     }
    // }
    // if !built {
    //     warn!("Failed to build interpreter, Won't emit js/html");
    // }

    // Get the final commit message we ended up on
    let commit_final_message = run("git", &["log", "--oneline", "-n", "1"])?;

    // Compute the source files that changed, and use that to filter the files
    // we copy over. We can't compare the generated tests, because for a
    // generated WPT we need to copy both the .js and .html even if only
    // one of those is different from the master.
    let tests_changed = find_tests_changed(repo)?;
    info!("Changed tests: {:#?}", tests_changed);

    // Include the changed tests, specified files, and `harness/` directory
    let mut included_files = Vec::new();
    included_files.extend_from_slice(&tests_changed);
    included_files.extend_from_slice(&config.included_tests);
    included_files.extend_from_slice(&repo.included_tests);
    included_files.push("harness/".to_owned());

    // Exclude files specified from the config and repo
    let mut excluded_files = Vec::new();
    excluded_files.extend_from_slice(&config.excluded_tests);
    excluded_files.extend_from_slice(&repo.excluded_tests);

    // Generate a regex set of the files to include or exclude
    let include = RegexSetBuilder::new(&included_files).build().unwrap();
    let exclude = RegexSetBuilder::new(&excluded_files).build().unwrap();

    // Copy over all the desired test-suites
    if !repo.skip_wast {
        copy_tests(repo, "test/core", "../tests", "wast", &include, &exclude);
    }
    if built && !repo.skip_js {
        copy_tests(repo, "js", "../tests", "js", &include, &exclude);
        copy_directives(repo, config)?;
    }

    Ok(Status {
        commit_final_message,
        commit_base_hash,
        merged,
        built,
    })
}

fn try_merge_parent(repo: &Repo, commit_base_hash: &str) -> Result<Merge> {
    if !repo.parent.is_some() {
        return Ok(Merge::Standalone);
    }
    let parent = repo.parent.as_ref().unwrap();

    // Try to merge with the parent branch.
    let message = format!("Merging {}:{}with {}", repo.name, commit_base_hash, parent);
    Ok(
        if !run("git", &["merge", "-q", parent, "-m", &message]).is_ok() {
            // Ignore merge conflicts in the document directory.
            if !run("git", &["checkout", "--ours", "document"]).is_ok()
                || !run("git", &["add", "document"]).is_ok()
                || !run("git", &["-c", "core.editor=true", "merge", "--continue"]).is_ok()
            {
                // Reset to master if we failed
                warn!(
                    "Failed to merge {}, falling back to {}.",
                    repo.name, &commit_base_hash
                );
                run("git", &["merge", "--abort"])?;
                run("git", &["reset", &commit_base_hash, "--hard"])?;
                Merge::Conflicted
            } else {
                Merge::Merged
            }
        } else {
            Merge::Merged
        },
    )
}

fn try_build_tests() -> Result<()> {
    let _ = fs::remove_dir_all("./js");
    fs::create_dir("./js")?;

    let paths = find("./test/core/");
    for path in paths {
        if path.extension() != Some(OsStr::new("wast")) {
            continue;
        }

        let source = std::fs::read_to_string(&path)?;
        let script = wast2js::convert(&path, &source)?;

        std::fs::write(
            Path::new("./js").join(&path.with_extension("wast.js").file_name().unwrap()),
            &script,
        )?;
    }

    fs::create_dir("./js/harness")?;
    write_string("./js/harness/harness.js", &wast2js::harness())?;

    Ok(())
}

fn copy_tests(
    repo: &Repo,
    src_dir: &str,
    dst_dir: &str,
    test_name: &str,
    include: &RegexSet,
    exclude: &RegexSet,
) {
    for path in find(src_dir) {
        let stripped_path = path.strip_prefix(src_dir).unwrap();
        let stripped_path_str = stripped_path.to_str().unwrap();

        if !include.is_match(stripped_path_str) || exclude.is_match(stripped_path_str) {
            continue;
        }

        let out_path = Path::new(dst_dir)
            .join(test_name)
            .join(&repo.name)
            .join(&stripped_path);
        let out_dir = out_path.parent().unwrap();
        let _ = fs::create_dir_all(out_dir);
        fs::copy(path, out_path).unwrap();
    }
}

fn copy_directives(repo: &Repo, config: &Config) -> Result<()> {
    // Write directives files
    if let Some(harness_directive) = &config.harness_directive {
        let directives_path = Path::new("../tests/js")
            .join(&repo.name)
            .join("harness/directives.txt");
        write_string(&directives_path, harness_directive)?;
    }
    let directives = format!(
        "{}{}",
        config.directive.as_ref().map(|x| x.as_str()).unwrap_or(""),
        repo.directive.as_ref().map(|x| x.as_str()).unwrap_or("")
    );
    if !directives.is_empty() {
        let directives_path = Path::new("../tests/js")
            .join(&repo.name)
            .join("directives.txt");
        write_string(&directives_path, &directives)?;
    }
    Ok(())
}

fn find_tests_changed(repo: &Repo) -> Result<Vec<String>> {
    let files_changed = if let Some(parent) = repo.parent.as_ref() {
        run(
            "git",
            &["diff", "--name-only", &repo.name, &parent, "test/core"],
        )?
        .lines()
        .map(|x| PathBuf::from(x))
        .collect()
    } else {
        find("test/core")
    };

    let mut tests_changed = Vec::new();
    for path in files_changed {
        if path.extension().map(|x| x.to_str().unwrap()) != Some("wast") {
            continue;
        }

        let name = path.file_name().unwrap().to_str().unwrap().to_owned();
        tests_changed.push(name);
    }
    Ok(tests_changed)
}
