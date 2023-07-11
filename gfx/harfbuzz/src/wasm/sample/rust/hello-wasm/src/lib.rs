use harfbuzz_wasm::{Font, GlyphBuffer};
use tiny_rng::{Rand, Rng};
use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn shape(
    _shape_plan: u32,
    font_ref: u32,
    buf_ref: u32,
    _features: u32,
    _num_features: u32,
) -> i32 {
    let mut rng = Rng::from_seed(123456);
    let font = Font::from_ref(font_ref);
    font.shape_with(buf_ref, "ot");
    let mut buffer = GlyphBuffer::from_ref(buf_ref);
    for mut item in buffer.glyphs.iter_mut() {
        // Randomize it!
        item.x_offset = ((rng.rand_u32() as i32) >> 24) - 120;
        item.y_offset = ((rng.rand_u32() as i32) >> 24) - 120;
    }
    // Buffer is written back to HB on drop
    1
}
