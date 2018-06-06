#! /usr/bin/env node
"use strict";

/* eslint-disable no-console, mozilla/no-task */
/* this is a node script; primary interaction is via console */

const Task = require("co-task");
const process = require("process");
const path = require("path");
const GitHubApi = require("@octokit/rest");
const shelljs = require("shelljs");
const child_process = require("child_process");
const github = new GitHubApi();

// some of our API requests need to be authenticated
let token = process.env.AS_PINE_TOKEN;
github.authenticate({type: "token", token});

// note that this token MUST have the public_repo scope set in the github API

const AS_REPO_OWNER = process.env.AS_REPO_OWNER || "mozilla";
const AS_REPO_NAME = process.env.AS_REPO_NAME || "activity-stream";
const AS_REPO = `${AS_REPO_OWNER}/${AS_REPO_NAME}`;
const OLDEST_PR_DATE = "2017-03-17";
const HG = "hg"; // mercurial
const HG_BRANCH_NAME = "pine";
const ALREADY_PUSHED_LABEL = "pushed-to-pine";
const TREEHERDER_PREFIX = "https://treeherder.mozilla.org/#/jobs?repo=pine&revision=";

// Path to the working directory where the export/commit operations will be
// done.  Highly advisted to be used only for this testing purpose so you don't
// accidently clobber real work.
//
// There will be two child directories:
//
// activity-stream - the github repo to be exported from.  MUST
//
// * be cloned by hand before running this script
// * be 'npm install'ed
// * have the ${ALREADY_PUSHED_LABEL} label created by hand
// * have the user who has issued AS_PINE_TOKEN as a collaborator for the repo
//   in order to be able to change labels.
//
// mozilla-central - the hg repo for firefox. Will be created if it doesn't
// already exist.
const {AS_PINE_TEST_DIR} = process.env;

const TESTING_LOCAL_MC = path.join(AS_PINE_TEST_DIR, "mozilla-central");

const SimpleGit = require("simple-git");
const TESTING_LOCAL_GIT = path.join(AS_PINE_TEST_DIR, AS_REPO_NAME);
const git = new SimpleGit(TESTING_LOCAL_GIT);

// Mostly useful to specify during development of the test automation so that
// prepare-mochitests-dev and friends from the development repo get used
// instead of from the testing repo, which won't have had any changes checked in
// just yet.
const AS_GIT_BIN_REPO = process.env.AS_GIT_BIN_REPO || TESTING_LOCAL_GIT;

const PREPARE_MOCHITESTS_DEV =
  path.join(AS_GIT_BIN_REPO, "bin", "prepare-mochitests-dev");

/**
 * Find all PRs merged since ${OLDEST_PR_DATE} that don't have
 * ${ALREADY_PUSHED_LABEL}
 *
 * @return {Promise} Promise that resolves with the search results or rejects
 */
function findNewlyMergedPRs() {
  const searchTerms = [
    // closed PRs in our repo
    `repo:${AS_REPO}`, "type:pr", "state:closed", "is:merged",

    // don't try and mochitest old closed stuff, we don't want to kick off a
    // zillion test jobs
    `merged:>=${OLDEST_PR_DATE}`,

    // only look at merges to master
    "base:master",

    // if it's already been pushed to pine, don't do it again
    `-label:${ALREADY_PUSHED_LABEL}`
  ];

  console.log(`Searching ${AS_REPO} for newly merged PRs`);
  return github.search.issues({q: searchTerms.join("+")});
}

/**
 * Return the commitId when the given PR was merged.  This is the one
 * we will want to export and test.
 *
 * @param  {String} prNumber  The number of the PR to export.
 * @return {String}           The commitId associated with the merge of this PR.
 */
function getPRMergeCommitId(prNumber) {
  return github.issues.getEvents({
    owner: AS_REPO_OWNER,
    repo: AS_REPO_NAME,
    issue_number: prNumber
  }).then(({data}) => {
    if (data.incomplete_results) {
      // XXX should handle this case theoretically, but since we'll be running
      // regularly from cron, it seems unlikely that we'll even hit 30 new
      // merges (default GitHub page size) in a single run.
      throw new Error("data.incomplete_results is true, aborting");
    }

    let mergeEvents = data.filter(item => item.event === "merged");
    if (mergeEvents.length > 1) {
      throw new Error("more than one merge event, aborting");
    } else if (!mergeEvents.length) {
      throw new Error(`Github returned no merge events for PR ${prNumber}, aborting.  Workaround: mark this PR as pushed-to-pine, so it gets skipped`);
    }
    let [mergeEvent] = mergeEvents;

    if (!mergeEvent.commit_id) {
      throw new Error("merge event has no commit id attached, aborted");
    }

    return mergeEvent.commit_id;
  }).catch(err => { throw err; });
}

/**
 * Checks out the given commit into ${TESTING_LOCAL_GIT}
 *
 * @param  {String} commitId
 * @return {Promise<String[]|?>} Resolves with commit [id, message] on checkout, or
 *                      rejects with error
 */
function checkoutGitCommit(commitId) {
  return new Promise((resolve, reject) => {
    console.log(`Fetching changes from github remote ${AS_REPO}...`);
    // fetch any changes from the remote
    git.fetch({}, (err, data) => {
      if (err) {
        reject(err);
        return;
      }
      console.log(`Starting github checkout of ${commitId}...`);
      git.checkout(commitId, (err2, data2) => {
        if (err2) {
          reject(err2);
          return;
        }

        // Pass along the original commit message
        git.show(["-s", "--format=%B"], (err3, data3) => {
          if (err3) {
            reject(err3);
            return;
          }
          resolve([commitId, data3.trim()]);
        });
      });
    });
  });
}

function exportToLocalMC(commitId) {
  return new Promise((resolve, reject) => {
    console.log("Preparing mochitest dev environment...");
    // Weirdly, /bin/yes causes npm-run-all bundle-static to explode, so we
    // use echo.
    shelljs.exec(`
      echo yes | \
        env AS_GIT_BIN_REPO=${AS_GIT_BIN_REPO} SYMLINK_TESTS=false \
        ENABLE_MC_AS=1 ${PREPARE_MOCHITESTS_DEV}`,
      {async: true, cwd: TESTING_LOCAL_GIT, silent: false}, (code, stdout, stderr) => {
        if (code) {
          reject(new Error(`${PREPARE_MOCHITESTS_DEV} failed, exit code: ${code}`));
          return;
        }

        resolve(commitId);
      });
  });
}

function commitToHg([commitId, commitMsg]) {
  return new Promise((resolve, reject) => {
    // we use child_process.execFile here because shelljs.exec goes through
    // the shell, which means that if the original commit message has shell
    // quote characters, things can go haywire in weird ways.
    console.log(`Committing exported ${commitId} to ${AS_REPO_NAME}...`);
    child_process.execFile(HG,
      [
        "commit",
        "--addremove",
        "-m",
        `${commitMsg}\n\nExport of ${commitId} from ${AS_REPO_OWNER}/${AS_REPO_NAME}`,
        "."
      ],
      {cwd: TESTING_LOCAL_MC, env: process.env, timeout: 5 * 60 * 1000},
      (code, stdout, stderr) => {
        if (code) {
          reject(new Error(`${HG} commit failed, output: ${stderr}`));
          return;
        }

        resolve(code);
      }
    );
  });
}

/**
 * [pushToHgProjectBranch description]
 *
 * @return {Promise<String|Number>} resolves with the text written to XXXstdout, or
 *                                  rejects with the exit code from ${HG}.
 */
function pushToHgProjectBranch() {
  return new Promise((resolve, reject) => {
    shelljs.exec(`${HG} push -f ${HG_BRANCH_NAME}`, {async: true, cwd: TESTING_LOCAL_MC},
      (code, stdout, stderr) => {
        if (code) {
          reject(new Error(`${HG} failed, exit code: ${code}`));
          return;
        }

        // Grab the last linked revision from the push output
        const [rev] = stdout.split(/(?:\/rev\/|changeset=)/).slice(-1)[0].split("\n");
        resolve(`[Treeherder: ${rev}](${TREEHERDER_PREFIX}${rev})`);
      }
    );
  });
}

/**
 * Remove last commit from the repo so the next artifact build will work right
 */
function stripTipFromHg() {
  return new Promise((resolve, reject) => {
    console.log("Stripping tip commit from mozilla-central so the next artifact build will work ...");
    shelljs.exec(`${HG} strip --force --rev -1`,
      {async: true, cwd: TESTING_LOCAL_MC},
      (code, stdout, stderr) => {
        if (code) {
          reject(new Error(`${HG} strip failed, output: ${stderr}`));
          return;
        }

        resolve(code);
      }
    );
  });
}

function annotateGithubPR(prNumber, annotation) {
  console.log(`Annotating ${prNumber} with ${annotation}...`);

  // We use createComment from issues instead of pullRequests because we're
  // not commenting on a particular commit
  return github.issues.createComment({
    owner: AS_REPO_OWNER,
    repo: AS_REPO_NAME,
    number: prNumber,
    body: annotation
  }).catch(reason => console.log(reason));
}

/**
 * Labels a given github PR ${ALREADY_PUSHED_LABEL}.
 */
function labelGithubPR(prNumber) {
  console.log(`Labeling PR ${prNumber} with ${ALREADY_PUSHED_LABEL}...`);

  return github.issues.addLabels({
    owner: AS_REPO_OWNER,
    repo: AS_REPO_NAME,
    number: prNumber,
    labels: [ALREADY_PUSHED_LABEL]
  }).catch(reason => console.log(reason));
}

function pushPR(pr) {
  return getPRMergeCommitId(pr.number)

    // get the merged commit to test
    .then(checkoutGitCommit)

    // use prepare-mochitest-dev to export
    .then(exportToLocalMC)

    // commit latest export to hg
    .then(commitToHg)

    // hg push
    .then(() => pushToHgProjectBranch().catch(() => {
      stripTipFromHg();
      throw new Error("pushToHgProjectBranch failed; tip stripped from hg");
    }))

    // annotate PR with URL to watch
    .then(annotation => annotateGithubPR(pr.number, annotation))

    // make sure next artifact build doesn't explode
    .then(() => stripTipFromHg())

    // label with ${ALREADY_PUSHED_LABEL}
    .then(() => labelGithubPR(pr.number))

    .catch(err => {
      console.log(err);
      throw err;
    });
}

function main() {
  findNewlyMergedPRs().then(({data}) => {
    if (data.incomplete_results) {
      throw new Error("data.incomplete_results is true, aborting");
    }

    if (data.items.length === 0) {
      console.log("No newly merged PRs to test");
      return;
    }

    function* executePush() {
      for (let pr of data.items) {
        yield pushPR(pr);
      }
    }

    // Serialize the execution of the export and pushing tests since each
    // depend on exclusive access the state of the git and hg repos used to
    // stage the tests.
    Task.spawn(executePush).then(() => {
      console.log("Processed all new merges.");
    }).catch(reason => {
      console.log("Something went wrong processing the merges:", reason);
      process.exitCode = -1;
    });
  })
  .catch(reason => {
    console.error(reason);
    process.exitCode = -1;
  });
}

main();
