use async_trait::async_trait;
use fluent_fallback::types::ResourceId;
use std::io;

/// The users of [`FileSource`] implement this trait to provide loading of
/// resources, returning the contents of a resource as a
/// `String`. [`FileSource`] handles the conversion from string representation
/// into `FluentResource`.
///
/// [`FileSource`]: source/struct.FileSource.html
#[async_trait(?Send)]
pub trait FileFetcher {
    /// Return the `String` representation for `path`. This version is
    /// blocking.
    ///
    /// See [`fetch`](#tymethod.fetch).
    fn fetch_sync(&self, resource_id: &ResourceId) -> io::Result<String>;

    /// Return the `String` representation for `path`.
    ///
    /// On success, returns `Poll::Ready(Ok(..))`.
    ///
    /// If no resource is available to be fetched, the method returns
    /// `Poll::Pending` and arranges for the current task (via
    /// `cx.waker().wake_by_ref()`) to receive a notification when the resource
    /// is available.
    ///
    /// See [`fetch_sync`](#tymethod.fetch_sync)
    async fn fetch(&self, path: &ResourceId) -> io::Result<String>;
}
