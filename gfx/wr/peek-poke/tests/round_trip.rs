// Copyright 2019 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use peek_poke::{Peek, PeekPoke, Poke};
use std::{fmt::Debug, marker::PhantomData};

fn poke_into<V: Peek + Poke>(a: &V) -> Vec<u8> {
    let mut v = <Vec<u8>>::with_capacity(<V>::max_size());
    let end_ptr = unsafe { a.poke_into(v.as_mut_ptr()) };
    let new_size = end_ptr as usize - v.as_ptr() as usize;
    assert!(new_size <= v.capacity());
    unsafe {
        v.set_len(new_size);
    }
    v
}

fn the_same<V>(a: V)
where
    V: Debug + Default + PartialEq + Peek + Poke,
{
    let v = poke_into(&a);
    let (b, end_ptr) = unsafe { peek_poke::peek_from_default(v.as_ptr()) };
    let size = end_ptr as usize - v.as_ptr() as usize;
    assert_eq!(size, v.len());
    assert_eq!(a, b);
}

#[test]
fn test_numbers() {
    // unsigned positive
    the_same(5u8);
    the_same(5u16);
    the_same(5u32);
    the_same(5u64);
    the_same(5usize);
    // signed positive
    the_same(5i8);
    the_same(5i16);
    the_same(5i32);
    the_same(5i64);
    the_same(5isize);
    // signed negative
    the_same(-5i8);
    the_same(-5i16);
    the_same(-5i32);
    the_same(-5i64);
    the_same(-5isize);
    // floating
    the_same(-100f32);
    the_same(0f32);
    the_same(5f32);
    the_same(-100f64);
    the_same(5f64);
}

#[test]
fn test_bool() {
    the_same(true);
    the_same(false);
}

#[test]
fn test_fixed_size_array() {
    the_same([24u32; 32]);
    the_same([1u64, 2, 3, 4, 5, 6, 7, 8]);
    the_same([0u8; 19]);
}

#[test]
fn test_tuple() {
    the_same((1isize, ));
    the_same((1isize, 2isize, 3isize));
    the_same((1isize, ()));
}

#[test]
fn test_basic_struct() {
    #[derive(Copy, Clone, Debug, Default, PartialEq, PeekPoke)]
    struct Bar {
        a: u32,
        b: u32,
        c: u32,
    }

    the_same(Bar {
        a: 2,
        b: 4,
        c: 42,
    });
}

#[test]
fn test_enum() {
    #[derive(Clone, Copy, Debug, PartialEq, PeekPoke)]
    enum TestEnum {
        NoArg,
        OneArg(usize),
        Args(usize, usize),
        AnotherNoArg,
        StructLike { x: usize, y: f32 },
    }

    impl Default for TestEnum {
        fn default() -> Self {
            TestEnum::NoArg
        }
    }

    the_same(TestEnum::NoArg);
    the_same(TestEnum::OneArg(4));
    the_same(TestEnum::Args(4, 5));
    the_same(TestEnum::AnotherNoArg);
    the_same(TestEnum::StructLike { x: 4, y: 3.14159 });
}

#[test]
fn test_enum_cstyle() {
    #[repr(u32)]
    #[derive(Clone, Copy, Debug, PartialEq, Eq, PeekPoke)]
    enum BorderStyle {
        None = 0,
        Solid = 1,
        Double = 2,
        Dotted = 3,
        Dashed = 4,
        Hidden = 5,
        Groove = 6,
        Ridge = 7,
        Inset = 8,
        Outset = 9,
    }

    impl Default for BorderStyle {
        fn default() -> Self {
            BorderStyle::None
        }
    }

    the_same(BorderStyle::None);
    the_same(BorderStyle::Solid);
    the_same(BorderStyle::Double);
    the_same(BorderStyle::Dotted);
    the_same(BorderStyle::Dashed);
    the_same(BorderStyle::Hidden);
    the_same(BorderStyle::Groove);
    the_same(BorderStyle::Ridge);
    the_same(BorderStyle::Inset);
    the_same(BorderStyle::Outset);
}

#[test]
fn test_phantom_data() {
    struct Bar;
    #[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PeekPoke)]
    struct Foo {
        x: u32,
        y: u32,
        _marker: PhantomData<Bar>,
    }
    the_same(Foo {
        x: 19,
        y: 42,
        _marker: PhantomData,
    });
}

#[test]
fn test_generic() {
    #[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PeekPoke)]
    struct Foo<T> {
        x: T,
        y: T,
    }
    the_same(Foo { x: 19.0, y: 42.0 });
}

#[test]
fn test_generic_enum() {
    #[derive(Clone, Copy, Debug, Default, PartialEq, PeekPoke)]
    pub struct PropertyBindingKey<T> {
        pub id: usize,
        _phantom: PhantomData<T>,
    }

    #[derive(Clone, Copy, Debug, PartialEq, PeekPoke)]
    pub enum PropertyBinding<T> {
        Value(T),
        Binding(PropertyBindingKey<T>, T),
    }

    impl<T: Default> Default for PropertyBinding<T> {
        fn default() -> Self {
            PropertyBinding::Value(Default::default())
        }
    }
}
