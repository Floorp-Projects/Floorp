#![warn(missing_docs)]
#![allow(dead_code)]
//! Interface to Harfbuzz's WASM exports
//!
//! This crate is designed to make it easier to write a
//! WASM shaper for your font using Rust. It binds the
//! functions exported by Harfbuzz into the WASM runtime,
//! and wraps them in an ergonomic interface using Rust
//! structures. For example, here is a basic shaping engine:
//!
//!
//! ```rust
//! #[wasm_bindgen]
//! pub fn shape(font_ref: u32, buf_ref: u32) -> i32 {
//!     let font = Font::from_ref(font_ref);
//!     let mut buffer = GlyphBuffer::from_ref(buf_ref);
//!     for mut item in buffer.glyphs.iter_mut() {
//!         // Map character to glyph
//!         item.codepoint = font.get_glyph(codepoint, 0);
//!         // Set advance width
//!         item.h_advance = font.get_glyph_h_advance(item.codepoint);
//!     }
//!     1
//! }
//! ```
use std::ffi::{c_int, CStr, CString};

#[cfg(feature = "kurbo")]
use kurbo::BezPath;

// We don't use #[wasm_bindgen] here because that makes
// assumptions about Javascript calling conventions. We
// really do just want to import some C symbols and run
// them in unsafe-land!
extern "C" {
    fn face_get_upem(face: u32) -> u32;
    fn font_get_face(font: u32) -> u32;
    fn font_get_glyph(font: u32, unicode: u32, uvs: u32) -> u32;
    fn font_get_scale(font: u32, x_scale: *mut i32, y_scale: *mut i32);
    fn font_get_glyph_extents(font: u32, glyph: u32, extents: *mut CGlyphExtents) -> bool;
    fn font_glyph_to_string(font: u32, glyph: u32, str: *const u8, len: u32);
    fn font_get_glyph_h_advance(font: u32, glyph: u32) -> i32;
    fn font_get_glyph_v_advance(font: u32, glyph: u32) -> i32;
    fn font_copy_glyph_outline(font: u32, glyph: u32, outline: *mut CGlyphOutline) -> bool;
    fn face_copy_table(font: u32, tag: u32, blob: *mut Blob) -> bool;
    fn buffer_copy_contents(buffer: u32, cbuffer: *mut CBufferContents) -> bool;
    fn buffer_set_contents(buffer: u32, cbuffer: &CBufferContents) -> bool;
    fn debugprint(s: *const u8);
    fn shape_with(
        font: u32,
        buffer: u32,
        features: u32,
        num_features: u32,
        shaper: *const u8,
    ) -> i32;
}

/// An opaque reference to a font at a given size and
/// variation. It is equivalent to the `hb_font_t` pointer
/// in Harfbuzz.
#[derive(Debug)]
pub struct Font(u32);

impl Font {
    /// Initialize a `Font` struct from the reference provided
    /// by Harfbuzz to the `shape` function.
    pub fn from_ref(ptr: u32) -> Self {
        Self(ptr)
    }
    /// Call the given Harfbuzz shaper on a buffer reference.
    ///
    /// For example, `font.shape_with(buffer_ref, "ot")` will
    /// run standard OpenType shaping, allowing you to modify
    /// the buffer contents after glyph mapping, substitution
    /// and positioning has taken place.
    pub fn shape_with(&self, buffer_ref: u32, shaper: &str) {
        let c_shaper = CString::new(shaper).unwrap();
        unsafe {
            shape_with(self.0, buffer_ref, 0, 0, c_shaper.as_ptr() as *const u8);
        }
    }

    /// Return the font face object that this font belongs to.
    pub fn get_face(&self) -> Face {
        Face(unsafe { font_get_face(self.0) })
    }

    /// Map a Unicode codepoint to a glyph ID.
    ///
    /// The `uvs` parameter specifies a Unicode Variation
    /// Selector codepoint which is used in conjunction with
    /// [format 14 cmap tables](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-14-unicode-variation-sequences)
    /// to provide alternate glyph mappings for characters with
    /// Unicode Variation Sequences. Generally you will pass
    /// `0`.
    pub fn get_glyph(&self, unicode: u32, uvs: u32) -> u32 {
        unsafe { font_get_glyph(self.0, unicode, uvs) }
    }

    /// Get the extents for a given glyph ID, in its design position.
    pub fn get_glyph_extents(&self, glyph: u32) -> CGlyphExtents {
        let mut extents = CGlyphExtents::default();
        unsafe {
            font_get_glyph_extents(self.0, glyph, &mut extents);
        }
        extents
    }

    /// Get the default advance width for a given glyph ID.
    pub fn get_glyph_h_advance(&self, glyph: u32) -> i32 {
        unsafe { font_get_glyph_h_advance(self.0, glyph) }
    }

    /// Get the default vertical advance for a given glyph ID.
    fn get_glyph_v_advance(&self, glyph: u32) -> i32 {
        unsafe { font_get_glyph_v_advance(self.0, glyph) }
    }

    /// Get the name of a glyph.
    ///
    /// If no names are provided by the font, names of the form
    /// `gidXXX` are constructed.
    pub fn get_glyph_name(&self, glyph: u32) -> String {
        let mut s = [1u8; 32];
        unsafe {
            font_glyph_to_string(self.0, glyph, s.as_mut_ptr(), 32);
        }
        unsafe { CStr::from_ptr(s.as_ptr() as *const _) }
            .to_str()
            .unwrap()
            .to_string()
    }

    /// Get the X and Y scale factor applied to this font.
    ///
    /// This should be divided by the units per em value to
    /// provide a scale factor mapping from design units to
    /// user units. (See [`Face::get_upem`].)
    pub fn get_scale(&self) -> (i32, i32) {
        let mut x_scale: i32 = 0;
        let mut y_scale: i32 = 0;
        unsafe {
            font_get_scale(
                self.0,
                &mut x_scale as *mut c_int,
                &mut y_scale as *mut c_int,
            )
        };
        (x_scale, y_scale)
    }

    #[cfg(feature = "kurbo")]
    /// Get the outline of a glyph as a vector of bezier paths
    pub fn get_outline(&self, glyph: u32) -> Vec<BezPath> {
        let mut outline = CGlyphOutline {
            n_points: 0,
            points: std::ptr::null_mut(),
            n_contours: 0,
            contours: std::ptr::null_mut(),
        };
        let end_pts_of_contours: &[usize] = unsafe {
            font_copy_glyph_outline(self.0, glyph, &mut outline);
            std::slice::from_raw_parts(outline.contours, outline.n_contours as usize)
        };
        let points: &[CGlyphOutlinePoint] =
            unsafe { std::slice::from_raw_parts(outline.points, outline.n_points as usize) };
        let mut results: Vec<BezPath> = vec![];
        let mut start_pt: usize = 0;
        for end_pt in end_pts_of_contours {
            let this_contour = &points[start_pt..*end_pt];
            start_pt = *end_pt;
            let mut path = BezPath::new();
            let mut ix = 0;
            while ix < this_contour.len() {
                let point = &this_contour[ix];
                match point.pointtype {
                    PointType::MoveTo => path.move_to((point.x as f64, point.y as f64)),
                    PointType::LineTo => path.line_to((point.x as f64, point.y as f64)),
                    PointType::QuadraticTo => {
                        ix += 1;
                        let end_pt = &this_contour[ix];
                        path.quad_to(
                            (point.x as f64, point.y as f64),
                            (end_pt.x as f64, end_pt.y as f64),
                        );
                    }
                    PointType::CubicTo => {
                        ix += 1;
                        let mid_pt = &this_contour[ix];
                        ix += 1;
                        let end_pt = &this_contour[ix];
                        path.curve_to(
                            (point.x as f64, point.y as f64),
                            (mid_pt.x as f64, mid_pt.y as f64),
                            (end_pt.x as f64, end_pt.y as f64),
                        );
                    }
                }
                ix += 1;
            }
            path.close_path();
            results.push(path);
        }
        results
    }
}

/// An opaque reference to a font face, equivalent to the `hb_face_t` pointer
/// in Harfbuzz.
///
/// This is generally returned from [`Font::get_face`].
#[derive(Debug)]
pub struct Face(u32);

impl Face {
    /// Get a blob containing the contents of the given binary font table.
    pub fn reference_table(&self, tag: &str) -> Blob {
        let mut tag_u: u32 = 0;
        let mut chars = tag.chars();
        tag_u |= (chars.next().unwrap() as u32) << 24;
        tag_u |= (chars.next().unwrap() as u32) << 16;
        tag_u |= (chars.next().unwrap() as u32) << 8;
        tag_u |= chars.next().unwrap() as u32;
        let mut blob = Blob {
            data: std::ptr::null_mut(),
            length: 0,
        };
        unsafe {
            face_copy_table(self.0, tag_u, &mut blob);
        }
        blob
    }

    /// Get the face's design units per em.
    pub fn get_upem(&self) -> u32 {
        unsafe { face_get_upem(self.0) }
    }
}

/// Trait implemented by custom structs representing buffer items
pub trait BufferItem {
    /// Construct an item in your preferred representation out of the info and position data provided by Harfbuzz.
    fn from_c(info: CGlyphInfo, position: CGlyphPosition) -> Self;
    /// Return info and position data to Harfbuzz.
    fn to_c(self) -> (CGlyphInfo, CGlyphPosition);
}

/// Generic representation of a Harfbuzz buffer item.
///
/// By making this generic, we allow you to implement your own
/// representations of buffer items; for example, in your shaper,
/// you may want certain fields to keep track of the glyph's name,
/// extents, or shape, so you would want a custom struct to represent
/// buffer items. If you don't care about any of them, use the
/// supplied `GlyphBuffer` struct.
#[derive(Debug)]
pub struct Buffer<T: BufferItem> {
    _ptr: u32,
    /// Glyphs in the buffer
    pub glyphs: Vec<T>,
}

impl<T: BufferItem> Buffer<T> {
    /// Construct a buffer from the pointer Harfbuzz provides to the WASM.
    ///
    /// The `Buffer` struct implements Drop, meaning that when the shaping
    /// function is finished, the buffer contents are sent back to Harfbuzz.
    pub fn from_ref(ptr: u32) -> Self {
        let mut c_contents = CBufferContents {
            info: std::ptr::null_mut(),
            position: std::ptr::null_mut(),
            length: 0,
        };

        unsafe {
            buffer_copy_contents(ptr, &mut c_contents) || panic!("Couldn't copy buffer contents")
        };
        let positions: Vec<CGlyphPosition> = unsafe {
            std::slice::from_raw_parts(c_contents.position, c_contents.length as usize).to_vec()
        };
        let infos: Vec<CGlyphInfo> = unsafe {
            std::slice::from_raw_parts(c_contents.info, c_contents.length as usize).to_vec()
        };
        Buffer {
            glyphs: infos
                .into_iter()
                .zip(positions.into_iter())
                .map(|(i, p)| T::from_c(i, p))
                .collect(),
            _ptr: ptr,
        }
    }
}

impl<T: BufferItem> Drop for Buffer<T> {
    fn drop(&mut self) {
        let mut positions: Vec<CGlyphPosition>;
        let mut infos: Vec<CGlyphInfo>;
        let glyphs = std::mem::take(&mut self.glyphs);
        (infos, positions) = glyphs.into_iter().map(|g| g.to_c()).unzip();
        let c_contents = CBufferContents {
            length: positions.len() as u32,
            info: infos[..].as_mut_ptr(),
            position: positions[..].as_mut_ptr(),
        };
        unsafe {
            if !buffer_set_contents(self._ptr, &c_contents) {
                panic!("Couldn't set buffer contents");
            }
        }
    }
}

/// Some data provided by Harfbuzz.
#[derive(Debug)]
#[repr(C)]
pub struct Blob {
    /// Length of the blob in bytes
    pub length: u32,
    /// A raw pointer to the contents
    pub data: *mut u8,
}

/// Glyph information in a buffer item provided by Harfbuzz
///
/// You'll only need to interact with this if you're writing
/// your own buffer item structure.
#[repr(C)]
#[derive(Debug, Clone)]
pub struct CGlyphInfo {
    pub codepoint: u32,
    pub mask: u32,
    pub cluster: u32,
    pub var1: u32,
    pub var2: u32,
}

/// Glyph positioning information in a buffer item provided by Harfbuzz
///
/// You'll only need to interact with this if you're writing
/// your own buffer item structure.
#[derive(Debug, Clone)]
#[repr(C)]
pub struct CGlyphPosition {
    pub x_advance: i32,
    pub y_advance: i32,
    pub x_offset: i32,
    pub y_offset: i32,
    pub var: u32,
}

/// Glyph extents
#[derive(Debug, Clone, Default)]
#[repr(C)]
pub struct CGlyphExtents {
    /// The scaled left side bearing of the glyph
    pub x_bearing: i32,
    /// The scaled coordinate of the top of the glyph
    pub y_bearing: i32,
    /// The width of the glyph
    pub width: i32,
    /// The height of the glyph
    pub height: i32,
}

#[derive(Debug)]
#[repr(C)]
struct CBufferContents {
    length: u32,
    info: *mut CGlyphInfo,
    position: *mut CGlyphPosition,
}

/// Ergonomic representation of a Harfbuzz buffer item
///
/// Harfbuzz buffers are normally split into two arrays,
/// one representing glyph information and the other
/// representing glyph positioning. In Rust, this would
/// require lots of zipping and unzipping, so we zip them
/// together into a single structure for you.
#[derive(Debug, Clone, Copy)]
pub struct Glyph {
    /// The Unicode codepoint or glyph ID of the item
    pub codepoint: u32,
    /// The index of the cluster in the input text where this came from
    pub cluster: u32,
    /// The horizontal advance of the glyph
    pub x_advance: i32,
    /// The vertical advance of the glyph
    pub y_advance: i32,
    /// The horizontal offset of the glyph
    pub x_offset: i32,
    /// The vertical offset of the glyph
    pub y_offset: i32,
    /// You can use this for whatever you like
    pub flags: u32,
}
impl BufferItem for Glyph {
    fn from_c(info: CGlyphInfo, pos: CGlyphPosition) -> Self {
        Self {
            codepoint: info.codepoint,
            cluster: info.cluster,
            x_advance: pos.x_advance,
            y_advance: pos.y_advance,
            x_offset: pos.x_offset,
            y_offset: pos.y_offset,
            flags: 0,
        }
    }
    fn to_c(self) -> (CGlyphInfo, CGlyphPosition) {
        let info = CGlyphInfo {
            codepoint: self.codepoint,
            cluster: self.cluster,
            mask: 0,
            var1: 0,
            var2: 0,
        };
        let pos = CGlyphPosition {
            x_advance: self.x_advance,
            y_advance: self.y_advance,
            x_offset: self.x_offset,
            y_offset: self.y_offset,
            var: 0,
        };
        (info, pos)
    }
}

#[repr(C)]
#[allow(clippy::enum_variant_names)]
#[derive(Clone, Debug)]
enum PointType {
    MoveTo,
    LineTo,
    QuadraticTo,
    CubicTo,
}

#[repr(C)]
#[derive(Clone, Debug)]
struct CGlyphOutlinePoint {
    x: f32,
    y: f32,
    pointtype: PointType,
}

#[repr(C)]
struct CGlyphOutline {
    n_points: usize,
    points: *mut CGlyphOutlinePoint,
    n_contours: usize,
    contours: *mut usize,
}

/// Our default buffer item struct. See also [`Glyph`].
pub type GlyphBuffer = Buffer<Glyph>;

/// Write a string to the Harfbuzz debug log.
pub fn debug(s: &str) {
    let c_s = CString::new(s).unwrap();
    unsafe {
        debugprint(c_s.as_ptr() as *const u8);
    };
}
