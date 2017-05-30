#![feature(test)]

extern crate rand;
extern crate test;
extern crate webrender;
extern crate webrender_traits;

use rand::Rng;
use test::Bencher;
use webrender::TexturePage;
use webrender_traits::{DeviceUintSize as Size};

#[bench]
fn bench_coalesce(b: &mut Bencher) {
    let mut rng = rand::thread_rng();
    let mut page = TexturePage::new_dummy(Size::new(10000, 10000));
    let mut test_page = TexturePage::new_dummy(Size::new(10000, 10000));
    while page.allocate(&Size::new(rng.gen_range(1, 100), rng.gen_range(1, 100))).is_some() {}
    b.iter(|| {
        test_page.fill_from(&page);
        test_page.coalesce();
    });
}
