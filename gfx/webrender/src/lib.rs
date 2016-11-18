/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#![feature(mpsc_select)]

//! A GPU based Webrender.
//!
//! It serves as an experimental render backend for [Servo](https://servo.org/),
//! but it can also be used as such in a standalone application.
//!
//! # External dependencies
//! Webrender currently depends on [FreeType](https://www.freetype.org/)
//!
//! # Api Structure
//! The main entry point to webrender is the `webrender::renderer::Renderer`.
//!
//! By calling `Renderer::new(...)` you get a `Renderer`, as well as a `RenderApiSender`.
//! Your `Renderer` is responsible to render the previously processed frames onto the screen.
//!
//! By calling `yourRenderApiSenderInstance.create_api()`, you'll get a `RenderApi` instance,
//! which is responsible for the processing of new frames. A worker thread is used internally to
//! untie the workload from the application thread and therefore be able
//! to make better use of multicore systems.
//!
//! What is referred to as a `frame`, is the current geometry on the screen.
//! A new Frame is created by calling [set_root_stacking_context()][newframe] on the `RenderApi`.
//! When the geometry is processed, the application will be informed via a `RenderNotifier`,
//! a callback which you employ with [set_render_notifier][notifier] on the `Renderer`
//! More information about [stacking contexts][stacking_contexts].
//!
//! `set_root_stacking_context()` also needs to be supplied with `BuiltDisplayList`s.
//! These are obtained by finalizing a `DisplayListBuilder`. These are used to draw your geometry.
//! But it doesn't only contain trivial geometry, it can also store another StackingContext, as
//! they're nestable.
//!
//! Remember to insert the DisplayListId into the StackingContext as well, as they'll be referenced
//! from there. An Id for your DisplayList can be obtained by calling
//! `yourRenderApiInstance.next_display_list_id();`
//!
//! [stacking_contexts]: https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Positioning/Understanding_z_index/The_stacking_context
//! [newframe]: ../webrender_traits/struct.RenderApi.html#method.set_root_stacking_context
//! [notifier]: struct.Renderer.html#method.set_render_notifier

#[macro_use]
extern crate lazy_static;
#[macro_use]
extern crate log;
#[macro_use]
extern crate bitflags;

mod batch_builder;
mod debug_colors;
mod debug_font_data;
mod debug_render;
mod device;
mod frame;
mod freelist;
mod geometry;
mod gpu_store;
mod internal_types;
mod layer;
mod mask_cache;
mod prim_store;
mod profiler;
mod record;
mod render_backend;
mod resource_cache;
mod scene;
mod spring;
mod texture_cache;
mod tiling;
mod util;
pub mod bindings;

mod platform {
    #[cfg(target_os="macos")]
    pub use platform::macos::font;
    #[cfg(any(target_os = "android", all(unix, not(target_os = "macos"))))]
    pub use platform::unix::font;
    #[cfg(target_os = "windows")]
    pub use platform::windows::font;

    #[cfg(target_os="macos")]
    pub mod macos {
        pub mod font;
    }
    #[cfg(any(target_os = "android", all(unix, not(target_os = "macos"))))]
    pub mod unix {
        pub mod font;
    }
    #[cfg(target_os = "windows")]
    pub mod windows {
        pub mod font;
    }
}

pub mod renderer;

#[cfg(target_os="macos")]
extern crate core_graphics;
#[cfg(target_os="macos")]
extern crate core_text;
#[cfg(target_os="macos")]
extern crate core_foundation;

#[cfg(all(unix, not(target_os="macos")))]
extern crate freetype;

#[cfg(target_os = "windows")]
extern crate dwrote;
#[cfg(target_os="windows")]
extern crate kernel32;
#[cfg(target_os="windows")]
extern crate winapi;

extern crate app_units;
extern crate bincode;
extern crate euclid;
extern crate fnv;
extern crate gleam;
extern crate ipc_channel;
extern crate num_traits;
//extern crate notify;
extern crate time;
extern crate webrender_traits;
extern crate offscreen_gl_context;
extern crate byteorder;
extern crate rayon;

pub use renderer::{Renderer, RendererOptions};
