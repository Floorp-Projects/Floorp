# qcms

Firefox's library for transforming image data between ICC profiles.

## Example
```rust
    // Decode the jpeg
    let mut d = jpeg_decoder::Decoder::new(std::fs::File::open("/Users/jrmuizel/Desktop/DSCF2460.jpg").unwrap());
    let mut data = d.decode().unwrap();
    let info = d.info().unwrap();

    // Extract the profile after decode
    let profile = d.icc_profile().unwrap();

    // Create a new qcms Profile
    let input = qcms::Profile::new_from_slice(&profile).unwrap();
    let mut output = qcms::Profile::new_sRGB();
    output.precache_output_transform();

    // Create a transform between input and output profiles and apply it.
    let xfm = qcms::Transform::new(&input, &output, qcms::DataType::RGB8, qcms::Intent::default()).unwrap();
    xfm.apply(&mut data);

    // write the result to a PNG
    let mut encoder = png::Encoder::new(std::fs::File::create("out.png").unwrap(), info.width as u32, info.height as u32);
    encoder.set_color(png::ColorType::Rgb);
    encoder.set_srgb(png::SrgbRenderingIntent::Perceptual);
    let mut writer = encoder.write_header().unwrap();
    writer.write_image_data(&data).unwrap(); // Save
```
