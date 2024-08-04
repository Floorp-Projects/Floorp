wit_bindgen::generate!({
  world: "nora",
});

mod mixin_parser;
mod transformer;

struct MyHost;

impl Guest for MyHost {
    fn transform(source: _rt::String, metadata: _rt::String) -> Result<String, String> {
        transformer::transform(source, metadata)
    }
}

export!(MyHost);
