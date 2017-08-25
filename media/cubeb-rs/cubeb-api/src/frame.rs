//! Frame utilities

use ChannelLayout;

/// A `Frame` is a collection of samples which have a a specific
/// layout represented by `ChannelLayout`
pub trait Frame {
    fn layout() -> ChannelLayout;
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// A monaural frame.
pub struct MonoFrame<T> {
    /// Mono channel
    pub m: T
}

impl<T> Frame for MonoFrame<T> {
    fn layout() -> ChannelLayout {
        ChannelLayout::Mono
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// A stereo frame.
pub struct StereoFrame<T> {
    /// Left channel
    pub l: T,
    /// Right channel
    pub r: T
}

impl<T> Frame for StereoFrame<T> {
    fn layout() -> ChannelLayout {
        ChannelLayout::Stereo
    }
}
