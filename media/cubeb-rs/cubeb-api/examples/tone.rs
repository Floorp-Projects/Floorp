// Copyright © 2011 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

//! libcubeb api/function test. Plays a simple tone.
extern crate cubeb;

mod common;

use cubeb::SampleType;
use std::f32::consts::PI;
use std::thread;
use std::time::Duration;

const SAMPLE_FREQUENCY: u32 = 48000;
const STREAM_FORMAT: cubeb::SampleFormat = cubeb::SampleFormat::S16LE;

// store the phase of the generated waveform
struct Tone {
    position: isize
}

impl cubeb::StreamCallback for Tone {
    type Frame = cubeb::MonoFrame<i16>;

    fn data_callback(&mut self, _: &[cubeb::MonoFrame<i16>], output: &mut [cubeb::MonoFrame<i16>]) -> isize {

        // generate our test tone on the fly
        for f in output.iter_mut() {
            // North American dial tone
            let t1 = (2.0 * PI * 350.0 * self.position as f32 / SAMPLE_FREQUENCY as f32).sin();
            let t2 = (2.0 * PI * 440.0 * self.position as f32 / SAMPLE_FREQUENCY as f32).sin();

            f.m = i16::from_float(0.5 * (t1 + t2));

            self.position += 1;
        }

        output.len() as isize
    }

    fn state_callback(&mut self, state: cubeb::State) {
        println!("stream {:?}", state);
    }
}

fn main() {
    let ctx = common::init("Cubeb tone example").expect("Failed to create cubeb context");

    let params = cubeb::StreamParamsBuilder::new()
        .format(STREAM_FORMAT)
        .rate(SAMPLE_FREQUENCY)
        .channels(1)
        .layout(cubeb::ChannelLayout::Mono)
        .take();

    let stream_init_opts = cubeb::StreamInitOptionsBuilder::new()
        .stream_name("Cubeb tone (mono)")
        .output_stream_param(&params)
        .latency(4096)
        .take();

    let stream = ctx.stream_init(
        &stream_init_opts,
        Tone {
            position: 0
        }
    ).expect("Failed to create cubeb stream");

    stream.start().unwrap();
    thread::sleep(Duration::from_millis(500));
    stream.stop().unwrap();
}
