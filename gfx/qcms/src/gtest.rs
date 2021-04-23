#[cfg(all(test, feature = "c_bindings"))]
mod gtest {
    use crate::{
        c_bindings::*, iccread::*, transform::DataType::*, transform::*,
        transform_util::lut_inverse_interp16, Intent::Perceptual,
    };
    use libc::c_void;
    use std::ptr::null_mut;

    #[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
    use crate::transform_neon::{
        qcms_transform_data_bgra_out_lut_neon, qcms_transform_data_rgb_out_lut_neon,
        qcms_transform_data_rgba_out_lut_neon,
    };

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    use crate::{
        transform_avx::{
            qcms_transform_data_bgra_out_lut_avx, qcms_transform_data_rgb_out_lut_avx,
            qcms_transform_data_rgba_out_lut_avx,
        },
        transform_sse2::{
            qcms_transform_data_bgra_out_lut_sse2, qcms_transform_data_rgb_out_lut_sse2,
            qcms_transform_data_rgba_out_lut_sse2,
        },
    };

    #[test]
    fn test_lut_inverse_crash() {
        let lutTable1: [u16; 128] = [
            0x0000, 0x0000, 0x0000, 0x8000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        ];
        let lutTable2: [u16; 128] = [
            0xFFF0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
            0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        ];

        // Crash/Assert test

        lut_inverse_interp16(5, &lutTable1);
        lut_inverse_interp16(5, &lutTable2);
    }

    #[test]
    fn test_lut_inverse() {
        // mimic sRGB_v4_ICC mBA Output
        //
        //       XXXX
        //      X
        //     X
        // XXXX
        let mut value: u16;
        let mut lutTable: [u16; 256] = [0; 256];

        for i in 0..20 {
            lutTable[i] = 0;
        }

        for i in 20..200 {
            lutTable[i] = ((i - 20) * 0xFFFF / (200 - 20)) as u16;
        }

        for i in 200..lutTable.len() {
            lutTable[i] = 0xFFFF;
        }

        for i in 0..65535 {
            lut_inverse_interp16(i, &lutTable);
        }

        // Lookup the interesting points

        value = lut_inverse_interp16(0, &lutTable);
        assert!(value <= 20 * 256);

        value = lut_inverse_interp16(1, &lutTable);
        assert!(value > 20 * 256);

        value = lut_inverse_interp16(65535, &lutTable);
        assert!(value < 201 * 256);
    }

    // this test takes to long to run on miri
    #[cfg(not(miri))]
    #[test]
    fn test_lut_inverse_non_monotonic() {
        // Make sure we behave sanely for non monotic functions
        //   X  X  X
        //  X  X  X
        // X  X  X
        let mut lutTable: [u16; 256] = [0; 256];

        for i in 0..100 {
            lutTable[i] = ((i - 0) * 0xFFFF / (100 - 0)) as u16;
        }

        for i in 100..200 {
            lutTable[i] = ((i - 100) * 0xFFFF / (200 - 100)) as u16;
        }

        for i in 200..256 {
            lutTable[i] = ((i - 200) * 0xFFFF / (256 - 200)) as u16;
        }

        for i in 0..65535 {
            lut_inverse_interp16(i, &lutTable);
        }

        // Make sure we don't crash, hang or let sanitizers do their magic
    }
    /* qcms_data_create_rgb_with_gamma is broken
    #[test]
    fn profile_from_gamma() {

        let white_point = qcms_CIE_xyY { x: 0.64, y: 0.33, Y: 1.};
        let primaries = qcms_CIE_xyYTRIPLE {
            red: qcms_CIE_xyY { x: 0.64, y: 0.33, Y: 1.},
            green: qcms_CIE_xyY { x: 0.21, y: 0.71, Y: 1.},
            blue: qcms_CIE_xyY { x: 0.15, y: 0.06, Y: 1.}
        };
        let mut mem: *mut libc::c_void = std::ptr::null_mut();
        let mut size: size_t = 0;
        unsafe { qcms_data_create_rgb_with_gamma(white_point, primaries, 2.2, &mut mem, &mut size); }
        assert!(size != 0)
    }
    */

    #[test]
    fn alignment() {
        assert_eq!(std::mem::align_of::<qcms_transform>(), 16);
    }

    #[test]
    fn basic() {
        let sRGB_profile = crate::c_bindings::qcms_profile_sRGB();

        let Rec709Primaries = qcms_CIE_xyYTRIPLE {
            red: qcms_CIE_xyY {
                x: 0.6400f64,
                y: 0.3300f64,
                Y: 1.0f64,
            },
            green: qcms_CIE_xyY {
                x: 0.3000f64,
                y: 0.6000f64,
                Y: 1.0f64,
            },
            blue: qcms_CIE_xyY {
                x: 0.1500f64,
                y: 0.0600f64,
                Y: 1.0f64,
            },
        };
        let D65 = qcms_white_point_sRGB();
        let other = unsafe { qcms_profile_create_rgb_with_gamma(D65, Rec709Primaries, 2.2) };
        unsafe { qcms_profile_precache_output_transform(&mut *other) };

        let transform = unsafe {
            qcms_transform_create(&mut *sRGB_profile, RGB8, &mut *other, RGB8, Perceptual)
        };
        let mut data: [u8; 120] = [0; 120];

        unsafe {
            qcms_transform_data(
                &*transform,
                data.as_ptr() as *const libc::c_void,
                data.as_mut_ptr() as *mut libc::c_void,
                data.len() / 3,
            )
        };

        unsafe {
            qcms_transform_release(transform);
            qcms_profile_release(sRGB_profile);
            qcms_profile_release(other);
        }
    }

    #[test]
    fn gray_alpha() {
        let sRGB_profile = qcms_profile_sRGB();
        let other = unsafe { qcms_profile_create_gray_with_gamma(2.2) };
        unsafe { qcms_profile_precache_output_transform(&mut *other) };

        let transform = unsafe {
            qcms_transform_create(&mut *other, GrayA8, &mut *sRGB_profile, RGBA8, Perceptual)
        };
        assert!(!transform.is_null());

        let in_data: [u8; 4] = [0, 255, 255, 0];
        let mut out_data: [u8; 2 * 4] = [0; 8];
        unsafe {
            qcms_transform_data(
                &*transform,
                in_data.as_ptr() as *const libc::c_void,
                out_data.as_mut_ptr() as *mut libc::c_void,
                in_data.len() / 2,
            )
        };

        assert_eq!(out_data, [0, 0, 0, 255, 255, 255, 255, 0]);
        unsafe {
            qcms_transform_release(transform);
            qcms_profile_release(sRGB_profile);
            qcms_profile_release(other);
        }
    }
    #[test]
    fn samples() {
        use libc::c_void;
        use std::io::Read;

        let mut d = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        qcms_enable_iccv4();
        d.push("fuzz");
        d.push("samples");
        let samples = [
            "0220-ca351238d719fd07ef8607d326b398fe.icc",
            "0372-973178997787ee780b4b58ee47cad683.icc",
            "0744-0a5faafe175e682b10c590b03d3f093b.icc",
            "0316-eb3f97ab646cd7b66bee80bdfe6098ac.icc",
            "0732-80707d91aea0f8e64ef0286cc7720e99.icc",
            "1809-2bd4b77651214ca6110fdbee2502671e.icc",
        ];
        for s in samples.iter() {
            let mut p = d.clone();
            p.push(s);
            let mut file = std::fs::File::open(p.clone()).unwrap();
            let mut data = Vec::new();
            file.read_to_end(&mut data).unwrap();
            let profile =
                unsafe { qcms_profile_from_memory(data.as_ptr() as *const c_void, data.len()) };
            assert_ne!(profile, std::ptr::null_mut());
            unsafe { qcms_profile_release(profile) };
        }
    }

    #[test]
    fn v4() {
        use libc::c_void;
        use std::io::Read;

        let mut p = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        qcms_enable_iccv4();
        p.push("profiles");
        // this profile was made by taking the lookup table profile from
        // http://displaycal.net/icc-color-management-test/ and removing
        // the unneeed tables using lcms
        p.push("displaycal-lut-stripped.icc");

        let mut file = std::fs::File::open(p).unwrap();
        let mut data = Vec::new();
        file.read_to_end(&mut data).unwrap();
        let profile =
            unsafe { qcms_profile_from_memory(data.as_ptr() as *const c_void, data.len()) };
        assert_ne!(profile, std::ptr::null_mut());

        let srgb_profile = qcms_profile_sRGB();
        assert_ne!(srgb_profile, std::ptr::null_mut());

        unsafe { qcms_profile_precache_output_transform(&mut *srgb_profile) };

        let intent = unsafe { qcms_profile_get_rendering_intent(&*profile) };
        let transform =
            unsafe { qcms_transform_create(&*profile, RGB8, &*srgb_profile, RGB8, intent) };

        assert_ne!(transform, std::ptr::null_mut());

        const SRC_SIZE: usize = 4;
        let src: [u8; SRC_SIZE * 3] = [
            246, 246, 246, // gray
            255, 0, 0, // red
            0, 255, 255, // cyan
            255, 255, 0, // yellow
        ];
        let mut dst: [u8; SRC_SIZE * 3] = [0; SRC_SIZE * 3];

        // the reference values here should be adjusted if the accuracy
        // of the transformation changes
        let reference = [
            246, 246, 246, // gray
            255, 0, 0, // red
            248, 14, 22, // red
            0, 0, 255, // blue
        ];

        unsafe {
            qcms_transform_data(
                &*transform,
                src.as_ptr() as *const libc::c_void,
                dst.as_mut_ptr() as *mut libc::c_void,
                SRC_SIZE,
            );
        }

        assert_eq!(reference, dst);
        unsafe { qcms_transform_release(transform) }
        unsafe { qcms_profile_release(profile) }
        unsafe { qcms_profile_release(srgb_profile) }
    }

    fn CmpRgbChannel(reference: &[u8], test: &[u8], index: usize) -> bool {
        (reference[index] as i32 - test[index] as i32).abs() <= 1
    }

    fn CmpRgbBufferImpl(
        refBuffer: &[u8],
        testBuffer: &[u8],
        pixels: usize,
        kSwapRB: bool,
        hasAlpha: bool,
    ) -> bool {
        let pixelSize = if hasAlpha { 4 } else { 3 };
        if refBuffer[..pixels * pixelSize] == testBuffer[..pixels * pixelSize] {
            return true;
        }

        let kRIndex = if kSwapRB { 2 } else { 0 };
        let kGIndex = 1;
        let kBIndex = if kSwapRB { 0 } else { 2 };
        let kAIndex = 3;

        let mut remaining = pixels;
        let mut reference = &refBuffer[..];
        let mut test = &testBuffer[..];
        while remaining > 0 {
            if !CmpRgbChannel(reference, test, kRIndex)
                || !CmpRgbChannel(reference, test, kGIndex)
                || !CmpRgbChannel(reference, test, kBIndex)
                || (hasAlpha && reference[kAIndex] != test[kAIndex])
            {
                assert_eq!(test[kRIndex], reference[kRIndex]);
                assert_eq!(test[kGIndex], reference[kGIndex]);
                assert_eq!(test[kBIndex], reference[kBIndex]);
                if hasAlpha {
                    assert_eq!(test[kAIndex], reference[kAIndex]);
                }
                return false;
            }
            remaining -= 1;
            reference = &reference[pixelSize..];
            test = &test[pixelSize..];
        }

        true
    }

    fn GetRgbInputBufferImpl(kSwapRB: bool, kHasAlpha: bool) -> (usize, Vec<u8>) {
        let colorSamples = [0, 5, 16, 43, 101, 127, 182, 255];
        let colorSampleMax = colorSamples.len();
        let pixelSize = if kHasAlpha { 4 } else { 3 };
        let pixelCount = colorSampleMax * colorSampleMax * 256 * 3;

        let mut outBuffer = vec![0; pixelCount * pixelSize];

        let kRIndex = if kSwapRB { 2 } else { 0 };
        let kGIndex = 1;
        let kBIndex = if kSwapRB { 0 } else { 2 };
        let kAIndex = 3;

        // Sample every red pixel value with a subset of green and blue.
        // we use a u16 for r to avoid https://github.com/rust-lang/rust/issues/78283
        let mut color: &mut [u8] = &mut outBuffer[..];
        for r in 0..=255u16 {
            for &g in colorSamples.iter() {
                for &b in colorSamples.iter() {
                    color[kRIndex] = r as u8;
                    color[kGIndex] = g;
                    color[kBIndex] = b;
                    if kHasAlpha {
                        color[kAIndex] = 0x80;
                    }
                    color = &mut color[pixelSize..];
                }
            }
        }

        // Sample every green pixel value with a subset of red and blue.
        let mut color = &mut outBuffer[..];
        for &r in colorSamples.iter() {
            for g in 0..=255u16 {
                for &b in colorSamples.iter() {
                    color[kRIndex] = r;
                    color[kGIndex] = g as u8;
                    color[kBIndex] = b;
                    if kHasAlpha {
                        color[kAIndex] = 0x80;
                    }
                    color = &mut color[pixelSize..];
                }
            }
        }

        // Sample every blue pixel value with a subset of red and green.
        let mut color = &mut outBuffer[..];
        for &r in colorSamples.iter() {
            for &g in colorSamples.iter() {
                for b in 0..=255u16 {
                    color[kRIndex] = r;
                    color[kGIndex] = g;
                    color[kBIndex] = b as u8;
                    if kHasAlpha {
                        color[kAIndex] = 0x80;
                    }
                    color = &mut color[pixelSize..];
                }
            }
        }

        (pixelCount, outBuffer)
    }

    fn GetRgbInputBuffer() -> (usize, Vec<u8>) {
        GetRgbInputBufferImpl(false, false)
    }

    fn GetRgbaInputBuffer() -> (usize, Vec<u8>) {
        GetRgbInputBufferImpl(false, true)
    }

    fn GetBgraInputBuffer() -> (usize, Vec<u8>) {
        GetRgbInputBufferImpl(true, true)
    }

    fn CmpRgbBuffer(refBuffer: &[u8], testBuffer: &[u8], pixels: usize) -> bool {
        CmpRgbBufferImpl(refBuffer, testBuffer, pixels, false, false)
    }

    fn CmpRgbaBuffer(refBuffer: &[u8], testBuffer: &[u8], pixels: usize) -> bool {
        CmpRgbBufferImpl(refBuffer, testBuffer, pixels, false, true)
    }

    fn CmpBgraBuffer(refBuffer: &[u8], testBuffer: &[u8], pixels: usize) -> bool {
        CmpRgbBufferImpl(refBuffer, testBuffer, pixels, true, true)
    }

    fn ClearRgbBuffer(buffer: &mut [u8], pixels: usize) {
        for i in 0..pixels * 3 {
            buffer[i] = 0;
        }
    }

    fn ClearRgbaBuffer(buffer: &mut [u8], pixels: usize) {
        for i in 0..pixels * 4 {
            buffer[i] = 0;
        }
    }

    fn GetRgbOutputBuffer(pixels: usize) -> Vec<u8> {
        vec![0; pixels * 3]
    }

    fn GetRgbaOutputBuffer(pixels: usize) -> Vec<u8> {
        vec![0; pixels * 4]
    }

    struct QcmsProfileTest {
        in_profile: *mut Profile,
        out_profile: *mut Profile,
        transform: *mut qcms_transform,

        input: Vec<u8>,
        output: Vec<u8>,
        reference: Vec<u8>,

        pixels: usize,
        storage_type: DataType,
        precache: bool,
    }

    impl QcmsProfileTest {
        fn new() -> QcmsProfileTest {
            QcmsProfileTest {
                in_profile: null_mut(),
                out_profile: null_mut(),
                transform: null_mut(),
                input: Vec::new(),
                output: Vec::new(),
                reference: Vec::new(),

                pixels: 0,
                storage_type: RGB8,
                precache: false,
            }
        }

        fn SetUp(&mut self) {
            qcms_enable_iccv4();
        }

        unsafe fn TearDown(&mut self) {
            if self.in_profile != null_mut() {
                qcms_profile_release(self.in_profile)
            }

            if self.out_profile != null_mut() {
                qcms_profile_release(self.out_profile)
            }

            if self.transform != null_mut() {
                qcms_transform_release(self.transform)
            }
        }

        unsafe fn SetTransform(&mut self, transform: *mut qcms_transform) -> bool {
            if self.transform != null_mut() {
                qcms_transform_release(self.transform)
            }
            self.transform = transform;
            self.transform != null_mut()
        }

        unsafe fn SetTransformForType(&mut self, ty: DataType) -> bool {
            self.SetTransform(qcms_transform_create(
                &*self.in_profile,
                ty,
                &*self.out_profile,
                ty,
                Perceptual,
            ))
        }

        unsafe fn SetBuffers(&mut self, ty: DataType) -> bool {
            match ty {
                RGB8 => {
                    let (pixels, input) = GetRgbInputBuffer();
                    self.input = input;
                    self.pixels = pixels;
                    self.reference = GetRgbOutputBuffer(self.pixels);
                    self.output = GetRgbOutputBuffer(self.pixels)
                }
                RGBA8 => {
                    let (pixels, input) = GetBgraInputBuffer();
                    self.input = input;
                    self.pixels = pixels;
                    self.reference = GetRgbaOutputBuffer(self.pixels);
                    self.output = GetRgbaOutputBuffer(self.pixels);
                }
                BGRA8 => {
                    let (pixels, input) = GetRgbaInputBuffer();
                    self.input = input;
                    self.pixels = pixels;
                    self.reference = GetRgbaOutputBuffer(self.pixels);
                    self.output = GetRgbaOutputBuffer(self.pixels);
                }
                _ => unreachable!("Unknown type!"),
            }
            self.storage_type = ty;
            self.pixels > 0
        }

        unsafe fn ClearOutputBuffer(&mut self) {
            match self.storage_type {
                RGB8 => ClearRgbBuffer(&mut self.output, self.pixels),
                RGBA8 | BGRA8 => ClearRgbaBuffer(&mut self.output, self.pixels),
                _ => unreachable!("Unknown type!"),
            }
        }

        unsafe fn ProduceRef(&mut self, trans_fn: transform_fn_t) {
            trans_fn.unwrap()(
                &*self.transform,
                self.input.as_mut_ptr(),
                self.reference.as_mut_ptr(),
                self.pixels,
            )
        }

        fn CopyInputToRef(&mut self) {
            let pixelSize = match self.storage_type {
                RGB8 => 3,
                RGBA8 | BGRA8 => 4,
                _ => unreachable!("Unknown type!"),
            };
            self.reference
                .copy_from_slice(&self.input[..self.pixels * pixelSize])
        }

        unsafe fn ProduceOutput(&mut self, trans_fn: transform_fn_t) {
            self.ClearOutputBuffer();
            trans_fn.unwrap()(
                &*self.transform,
                self.input.as_mut_ptr(),
                self.output.as_mut_ptr(),
                self.pixels,
            )
        }

        unsafe fn VerifyOutput(&self, buf: &[u8]) -> bool {
            match self.storage_type {
                RGB8 => CmpRgbBuffer(buf, &self.output, self.pixels),
                RGBA8 => CmpRgbaBuffer(buf, &self.output, self.pixels),
                BGRA8 => CmpBgraBuffer(buf, &self.output, self.pixels),
                _ => unreachable!("Unknown type!"),
            }
        }

        unsafe fn ProduceVerifyOutput(&mut self, trans_fn: transform_fn_t) -> bool {
            self.ProduceOutput(trans_fn);
            self.VerifyOutput(&self.reference)
        }

        unsafe fn PrecacheOutput(&mut self) {
            qcms_profile_precache_output_transform(&mut *self.out_profile);
            self.precache = true;
        }
        unsafe fn TransformPrecache(&mut self) {
            assert_eq!(self.precache, false);
            assert!(self.SetBuffers(RGB8));
            assert!(self.SetTransformForType(RGB8));
            self.ProduceRef(Some(qcms_transform_data_rgb_out_lut));

            self.PrecacheOutput();
            assert!(self.SetTransformForType(RGB8));
            assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_precache)))
        }

        unsafe fn TransformPrecachePlatformExt(&mut self) {
            self.PrecacheOutput();

            // Verify RGB transforms.
            assert!(self.SetBuffers(RGB8));
            assert!(self.SetTransformForType(RGB8));
            self.ProduceRef(Some(qcms_transform_data_rgb_out_lut_precache));

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            {
                if is_x86_feature_detected!("sse2") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_sse2)));
                }
                if is_x86_feature_detected!("avx") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_avx)))
                }
            }

            #[cfg(target_arch = "arm")]
            {
                if is_arm_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_neon)))
                }
            }

            #[cfg(target_arch = "aarch64")]
            {
                if is_aarch64_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_neon)))
                }
            }

            // Verify RGBA transform.
            assert!(self.SetBuffers(RGBA8));
            assert!(self.SetTransformForType(RGBA8));
            self.ProduceRef(Some(qcms_transform_data_rgba_out_lut_precache));

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            {
                if is_x86_feature_detected!("sse2") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgba_out_lut_sse2)));
                }
                if is_x86_feature_detected!("avx") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgba_out_lut_avx)))
                }
            }

            #[cfg(target_arch = "arm")]
            {
                if is_arm_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgba_out_lut_neon)))
                }
            }

            #[cfg(target_arch = "aarch64")]
            {
                if is_aarch64_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgba_out_lut_neon)))
                }
            }

            // Verify BGRA transform.
            assert!(self.SetBuffers(BGRA8));
            assert!(self.SetTransformForType(BGRA8));
            self.ProduceRef(Some(qcms_transform_data_bgra_out_lut_precache));

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            {
                if is_x86_feature_detected!("sse2") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_bgra_out_lut_sse2)));
                }
                if is_x86_feature_detected!("avx") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_bgra_out_lut_avx)))
                }
            }

            #[cfg(target_arch = "arm")]
            {
                if is_arm_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_bgra_out_lut_neon)))
                }
            }

            #[cfg(target_arch = "aarch64")]
            {
                if is_aarch64_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_bgra_out_lut_neon)))
                }
            }
        }
    }

    #[test]
    fn sRGB_to_sRGB_precache() {
        unsafe {
            let mut pt = QcmsProfileTest::new();
            pt.SetUp();
            pt.in_profile = qcms_profile_sRGB();
            pt.out_profile = qcms_profile_sRGB();
            pt.TransformPrecache();
            pt.TearDown();
        }
    }

    #[test]
    fn sRGB_to_sRGB_transform_identity() {
        unsafe {
            let mut pt = QcmsProfileTest::new();
            pt.SetUp();
            pt.in_profile = qcms_profile_sRGB();
            pt.out_profile = qcms_profile_sRGB();
            pt.PrecacheOutput();
            pt.SetBuffers(RGB8);
            pt.SetTransformForType(RGB8);
            qcms_transform_data(
                &*pt.transform,
                pt.input.as_mut_ptr() as *mut c_void,
                pt.output.as_mut_ptr() as *mut c_void,
                pt.pixels,
            );
            assert!(pt.VerifyOutput(&pt.input));
            pt.TearDown();
        }
    }

    fn profile_from_path(file: &str) -> *mut Profile {
        use std::io::Read;
        let mut path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        path.push("profiles");
        path.push(file);
        let mut file = std::fs::File::open(path).unwrap();
        let mut data = Vec::new();
        file.read_to_end(&mut data).unwrap();
        let profile =
            unsafe { qcms_profile_from_memory(data.as_ptr() as *const c_void, data.len()) };
        assert_ne!(profile, std::ptr::null_mut());
        profile
    }

    #[test]
    fn sRGB_to_ThinkpadW540() {
        unsafe {
            let mut pt = QcmsProfileTest::new();
            pt.SetUp();
            pt.in_profile = qcms_profile_sRGB();
            pt.out_profile = profile_from_path("lcms_thinkpad_w540.icc");
            pt.TransformPrecachePlatformExt();
            pt.TearDown();
        }
    }

    #[test]
    fn sRGB_to_SamsungSyncmaster() {
        unsafe {
            let mut pt = QcmsProfileTest::new();
            pt.SetUp();
            pt.in_profile = qcms_profile_sRGB();
            pt.out_profile = profile_from_path("lcms_samsung_syncmaster.icc");
            pt.TransformPrecachePlatformExt();
            pt.TearDown();
        }
    }

    #[test]
    fn v4_output() {
        qcms_enable_iccv4();
        let input = qcms_profile_sRGB();
        // B2A0-ident.icc was created from the profile in bug 1679621
        // manually edited using iccToXML/iccFromXML
        let output = profile_from_path("B2A0-ident.icc");

        let transform = unsafe { qcms_transform_create(&*input, RGB8, &*output, RGB8, Perceptual) };
        let src = [0u8, 60, 195];
        let mut dst = [0u8, 0, 0];
        unsafe {
            qcms_transform_data(
                &*transform,
                src.as_ptr() as *const libc::c_void,
                dst.as_mut_ptr() as *mut libc::c_void,
                1,
            );
        }
        assert_eq!(dst, [15, 16, 122]);
        unsafe {
            qcms_transform_release(transform);
            qcms_profile_release(input);
            qcms_profile_release(output);
        }
    }

    #[test]
    fn gray_smoke_test() {
        let input = crate::Profile::new_gray_with_gamma(2.2);
        let output = crate::Profile::new_sRGB();
        let xfm =
            transform_create(&input, GrayA8, &output, RGBA8, crate::Intent::default()).unwrap();
        let src = [20u8, 20u8];
        let mut dst = [0u8, 0, 0, 0];
        unsafe {
            qcms_transform_data(
                &xfm,
                src.as_ptr() as *const libc::c_void,
                dst.as_mut_ptr() as *mut libc::c_void,
                src.len() / GrayA8.bytes_per_pixel(),
            );
        }
    }

    #[test]
    fn data_create_rgb_with_gamma() {
        let Rec709Primaries = qcms_CIE_xyYTRIPLE {
            red: {
                qcms_CIE_xyY {
                    x: 0.6400,
                    y: 0.3300,
                    Y: 1.0,
                }
            },
            green: {
                qcms_CIE_xyY {
                    x: 0.3000,
                    y: 0.6000,
                    Y: 1.0,
                }
            },
            blue: {
                qcms_CIE_xyY {
                    x: 0.1500,
                    y: 0.0600,
                    Y: 1.0,
                }
            },
        };
        let D65 = qcms_white_point_sRGB();
        let mut mem = std::ptr::null_mut();
        let mut size = 0;
        unsafe {
            qcms_data_create_rgb_with_gamma(D65, Rec709Primaries, 2.2, &mut mem, &mut size);
        }
        assert_ne!(size, 0);
        unsafe { libc::free(mem) };
    }
}

#[cfg(test)]
mod test {
    use crate::{Profile, Transform};
    #[test]
    fn identity() {
        let p1 = Profile::new_sRGB();
        let p2 = Profile::new_sRGB();
        let xfm =
            Transform::new(&p1, &p2, crate::DataType::RGB8, crate::Intent::default()).unwrap();
        let mut data = [4, 30, 80];
        xfm.apply(&mut data);
        assert_eq!(data, [4, 30, 80]);
    }
    #[test]
    fn D50() {
        let p1 = Profile::new_sRGB();
        let p2 = Profile::new_XYZD50();
        let xfm =
            Transform::new(&p1, &p2, crate::DataType::RGB8, crate::Intent::default()).unwrap();
        let mut data = [4, 30, 80];
        xfm.apply(&mut data);
        assert_eq!(data, [4, 4, 15]);
    }

    fn profile_from_path(file: &str) -> Box<Profile> {
        use std::io::Read;
        let mut path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        path.push("profiles");
        path.push(file);
        let mut file = std::fs::File::open(path).unwrap();
        let mut data = Vec::new();
        file.read_to_end(&mut data).unwrap();
        Profile::new_from_slice(&data).unwrap()
    }

    #[test]
    fn parametric_threshold() {
        let src = profile_from_path("parametric-thresh.icc");
        let dst = crate::Profile::new_sRGB();
        let xfm =
            Transform::new(&src, &dst, crate::DataType::RGB8, crate::Intent::default()).unwrap();
        let mut data = [4, 30, 80];
        xfm.apply(&mut data);
        assert_eq!(data, [188, 188, 189]);
    }

    #[test]
    fn cmyk() {
        let input = profile_from_path("ps_cmyk_min.icc");
        let output = Profile::new_sRGB();
        let xfm = crate::Transform::new_to(
            &input,
            &output,
            crate::DataType::CMYK,
            crate::DataType::RGB8,
            crate::Intent::default(),
        )
        .unwrap();
        let src = [4, 30, 80, 10];
        let mut dst = [0, 0, 0];
        xfm.convert(&src, &mut dst);
        assert_eq!(dst, [252, 237, 211]);

    }
}
