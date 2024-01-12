use std::{
    ffi::OsStr,
    fmt::{self, Display},
    fs,
    ops::Deref,
    path::{Path, PathBuf, StripPrefixError},
};

use miette::{ensure, Context, IntoDiagnostic};

#[derive(Debug)]
pub(crate) struct FileRoot {
    nickname: &'static str,
    path: PathBuf,
}

impl Eq for FileRoot {}

impl PartialEq for FileRoot {
    fn eq(&self, other: &Self) -> bool {
        self.path == other.path
    }
}

impl Ord for FileRoot {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.path.cmp(&other.path)
    }
}

impl PartialOrd for FileRoot {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl FileRoot {
    pub(crate) fn new<P>(nickname: &'static str, path: P) -> miette::Result<Self>
    where
        P: AsRef<Path>,
    {
        let path = path.as_ref();
        Ok(Self {
            nickname,
            path: dunce::canonicalize(path)
                .map_err(miette::Report::msg)
                .wrap_err_with(|| format!("failed to canonicalize {path:?}"))?,
        })
    }

    pub(crate) fn nickname(&self) -> &str {
        self.nickname
    }

    pub(crate) fn try_child<P>(&self, path: P) -> Result<Child<'_>, StripPrefixError>
    where
        P: AsRef<Path>,
    {
        let path = path.as_ref();
        if path.is_absolute() {
            path.strip_prefix(&self.path)?;
        }
        Ok(Child {
            root: self,
            path: self.path.join(path),
        })
    }

    #[track_caller]
    pub(crate) fn child<P>(&self, path: P) -> Child<'_>
    where
        P: AsRef<Path>,
    {
        self.try_child(path)
            .into_diagnostic()
            .wrap_err("invariant violation: `path` is absolute and not a child of this file root")
            .unwrap()
    }

    fn removed_dir<P>(&self, path: P) -> miette::Result<Child<'_>>
    where
        P: AsRef<Path>,
    {
        let path = path.as_ref();
        let child = self.child(path);
        if child.exists() {
            log::info!("removing old contents of {child}…",);
            log::trace!("removing directory {:?}", &*child);
            fs::remove_dir_all(&*child)
                .map_err(miette::Report::msg)
                .wrap_err_with(|| format!("failed to remove old contents of {child}"))?;
        }
        Ok(child)
    }

    fn removed_file<P>(&self, path: P) -> miette::Result<Child<'_>>
    where
        P: AsRef<Path>,
    {
        let path = path.as_ref();
        let child = self.child(path);
        if child.exists() {
            log::info!("removing old copy of {child}…",);
            fs::remove_file(&*child)
                .map_err(miette::Report::msg)
                .wrap_err_with(|| format!("failed to remove old copy of {child}"))?;
        }
        Ok(child)
    }

    pub(crate) fn regen_dir<P>(
        &self,
        path: P,
        gen: impl FnOnce(&Child<'_>) -> miette::Result<()>,
    ) -> miette::Result<Child<'_>>
    where
        P: AsRef<Path>,
    {
        let child = self.removed_dir(path)?;
        gen(&child)?;
        ensure!(
            child.is_dir(),
            "{} was not regenerated for an unknown reason",
            child,
        );
        Ok(child)
    }

    pub(crate) fn regen_file<P>(
        &self,
        path: P,
        gen: impl FnOnce(&Child<'_>) -> miette::Result<()>,
    ) -> miette::Result<Child<'_>>
    where
        P: AsRef<Path>,
    {
        let child = self.removed_file(path)?;
        gen(&child)?;
        ensure!(
            child.is_file(),
            "{} was not regenerated for an unknown reason",
            child,
        );
        Ok(child)
    }
}

impl Deref for FileRoot {
    type Target = Path;

    fn deref(&self) -> &Self::Target {
        &self.path
    }
}

impl AsRef<Path> for FileRoot {
    fn as_ref(&self) -> &Path {
        &self.path
    }
}

impl AsRef<OsStr> for FileRoot {
    fn as_ref(&self) -> &OsStr {
        self.path.as_os_str()
    }
}

impl Display for FileRoot {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let Self { nickname, path } = self;
        write!(f, "`{}` (AKA `<{nickname}>`)", path.display())
    }
}

#[derive(Debug, Eq, Ord, PartialEq, PartialOrd)]
pub(crate) struct Child<'a> {
    root: &'a FileRoot,
    /// NOTE: This is always an absolute path that is a child of the `root`.
    path: PathBuf,
}

impl Child<'_> {
    pub(crate) fn relative_path(&self) -> &Path {
        let Self { root, path } = self;
        path.strip_prefix(root).unwrap()
    }

    pub(crate) fn try_child<P>(&self, path: P) -> Result<Self, StripPrefixError>
    where
        P: AsRef<Path>,
    {
        let child_path = path.as_ref();
        let Self { root, path } = self;

        if child_path.is_absolute() {
            child_path.strip_prefix(path)?;
        }
        Ok(Child {
            root,
            path: path.join(child_path),
        })
    }

    #[track_caller]
    pub(crate) fn child<P>(&self, path: P) -> Self
    where
        P: AsRef<Path>,
    {
        self.try_child(path)
            .into_diagnostic()
            .wrap_err("invariant violation: `path` is absolute and not a child of this child")
            .unwrap()
    }
}

impl Deref for Child<'_> {
    type Target = Path;

    fn deref(&self) -> &Self::Target {
        &self.path
    }
}

impl AsRef<Path> for Child<'_> {
    fn as_ref(&self) -> &Path {
        &self.path
    }
}

impl AsRef<OsStr> for Child<'_> {
    fn as_ref(&self) -> &OsStr {
        self.path.as_os_str()
    }
}

impl Display for Child<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "`<{}>{}{}`",
            self.root.nickname(),
            std::path::MAIN_SEPARATOR,
            self.relative_path().display()
        )
    }
}

pub(crate) fn existing_file<P>(path: P) -> P
where
    P: AsRef<Path>,
{
    let p = path.as_ref();
    assert!(p.is_file(), "{p:?} does not exist as a file");
    path
}

pub(crate) fn copy_dir<P, Q>(source: P, dest: Q) -> miette::Result<()>
where
    P: Display + AsRef<Path>,
    Q: Display + AsRef<Path>,
{
    log::debug!(
        "copy-merging directories from {} into {}",
        source.as_ref().display(),
        dest.as_ref().display(),
    );
    ::dircpy::copy_dir(&source, &dest)
        .into_diagnostic()
        .wrap_err_with(|| format!("failed to copy files from {source} to {dest}"))
}

pub(crate) fn read_to_string<P>(path: P) -> miette::Result<String>
where
    P: AsRef<Path>,
{
    fs::read_to_string(&path)
        .into_diagnostic()
        .wrap_err_with(|| {
            format!(
                "failed to read UTF-8 string from path {}",
                path.as_ref().display()
            )
        })
}

pub(crate) fn copy<P1, P2>(from: P1, to: P2) -> miette::Result<u64>
where
    P1: AsRef<Path>,
    P2: AsRef<Path>,
{
    fs::copy(&from, &to).into_diagnostic().wrap_err_with(|| {
        format!(
            "failed to copy {} to {}",
            from.as_ref().display(),
            to.as_ref().display()
        )
    })
}

pub(crate) fn create_dir_all<P>(path: P) -> miette::Result<()>
where
    P: AsRef<Path>,
{
    fs::create_dir_all(&path)
        .into_diagnostic()
        .wrap_err_with(|| {
            format!(
                "failed to create directories leading up to {}",
                path.as_ref().display()
            )
        })
}

pub(crate) fn remove_file<P>(path: P) -> miette::Result<()>
where
    P: AsRef<Path>,
{
    fs::remove_file(&path)
        .into_diagnostic()
        .wrap_err_with(|| format!("failed to remove file at path {}", path.as_ref().display()))
}

pub(crate) fn write<P, C>(path: P, contents: C) -> miette::Result<()>
where
    P: AsRef<Path>,
    C: AsRef<[u8]>,
{
    fs::write(&path, &contents)
        .into_diagnostic()
        .wrap_err_with(|| format!("failed to write to path {}", path.as_ref().display()))
}
