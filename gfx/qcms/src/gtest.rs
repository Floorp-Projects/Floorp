#[cfg(test)]
mod test {
    use crate::{
        iccread::*, transform::*, transform_util::lut_inverse_interp16, QCMS_INTENT_DEFAULT,
        QCMS_INTENT_PERCEPTUAL,
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
        let mut lutTable1: [u16; 128] = [
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
        let mut lutTable2: [u16; 128] = [
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
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        unsafe {
            use crate::transform::qcms_enable_avx;
            if is_x86_feature_detected!("avx") {
                qcms_enable_avx()
            }
        };
        let sRGB_profile = unsafe { crate::iccread::qcms_profile_sRGB() };

        let mut Rec709Primaries = qcms_CIE_xyYTRIPLE {
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
        let D65 = unsafe { qcms_white_point_sRGB() };
        let other = unsafe { qcms_profile_create_rgb_with_gamma(D65, Rec709Primaries, 2.2) };
        unsafe { qcms_profile_precache_output_transform(other) };

        let transform = unsafe {
            qcms_transform_create(
                &mut *sRGB_profile,
                QCMS_DATA_RGB_8,
                &mut *other,
                QCMS_DATA_RGB_8,
                QCMS_INTENT_PERCEPTUAL,
            )
        };
        let mut data: [u8; 120] = [0; 120];

        unsafe {
            qcms_transform_data(
                transform,
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
        let sRGB_profile = unsafe { crate::iccread::qcms_profile_sRGB() };
        let other = unsafe { qcms_profile_create_gray_with_gamma(2.2) };
        unsafe { qcms_profile_precache_output_transform(other) };

        let transform = unsafe {
            qcms_transform_create(
                &mut *other,
                QCMS_DATA_GRAYA_8,
                &mut *sRGB_profile,
                QCMS_DATA_RGBA_8,
                QCMS_INTENT_PERCEPTUAL,
            )
        };
        assert!(!transform.is_null());

        let mut in_data: [u8; 4] = [0, 255, 255, 0];
        let mut out_data: [u8; 2 * 4] = [0; 8];
        unsafe {
            qcms_transform_data(
                transform,
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
        unsafe {
            qcms_enable_iccv4();
        }
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
        unsafe {
            qcms_enable_iccv4();
        }
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

        let srgb_profile = unsafe { qcms_profile_sRGB() };
        assert_ne!(srgb_profile, std::ptr::null_mut());

        unsafe { qcms_profile_precache_output_transform(srgb_profile) };

        let intent = unsafe { qcms_profile_get_rendering_intent(profile) };
        let transform = unsafe {
            qcms_transform_create(
                &*profile,
                QCMS_DATA_RGB_8,
                &*srgb_profile,
                QCMS_DATA_RGB_8,
                intent,
            )
        };

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
        let mut reference = [
            246, 246, 246, // gray
            255, 0, 0, // red
            248, 14, 22, // red
            0, 0, 255, // blue
        ];

        unsafe {
            qcms_transform_data(
                transform,
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
        let mut i = 0;
        let mut r_count = 0;
        let mut g_count = 0;
        let mut b_count = 0;
        for &r in colorSamples.iter() {
            for &g in colorSamples.iter() {
                for b in 0..=255u16 {
                    color[kRIndex] = r;
                    color[kGIndex] = g;
                    color[kBIndex] = b as u8;
                    if kHasAlpha {
                        color[kAIndex] = 0x80;
                    }
                    i += pixelSize;
                    color = &mut color[pixelSize..];
                    b_count += 1;
                }
                g_count += 1;
            }
            r_count += 1;
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
        in_profile: *mut qcms_profile,
        out_profile: *mut qcms_profile,
        transform: *mut qcms_transform,

        input: Vec<u8>,
        output: Vec<u8>,
        reference: Vec<u8>,

        pixels: usize,
        storage_type: qcms_data_type,
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
                storage_type: QCMS_DATA_RGB_8,
                precache: false,
            }
        }

        fn SetUp(&mut self) {
            unsafe {
                qcms_enable_iccv4();
            }

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            unsafe {
                if is_x86_feature_detected!("avx") {
                    qcms_enable_avx()
                }
            };

            #[cfg(target_arch = "arm")]
            unsafe {
                use crate::transform::qcms_enable_neon;
                if is_arm_feature_detected!("neon") {
                    qcms_enable_neon()
                }
            };

            #[cfg(target_arch = "aarch64")]
            unsafe {
                use crate::transform::qcms_enable_neon;
                if is_aarch64_feature_detected!("neon") {
                    qcms_enable_neon()
                }
            };
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
            !(self.transform == null_mut())
        }

        unsafe fn SetTransformForType(&mut self, ty: qcms_data_type) -> bool {
            self.SetTransform(qcms_transform_create(
                &*self.in_profile,
                ty,
                &*self.out_profile,
                ty,
                QCMS_INTENT_DEFAULT,
            ))
        }

        unsafe fn SetBuffers(&mut self, ty: qcms_data_type) -> bool {
            match ty {
                QCMS_DATA_RGB_8 => {
                    let (pixels, input) = GetRgbInputBuffer();
                    self.input = input;
                    self.pixels = pixels;
                    self.reference = GetRgbOutputBuffer(self.pixels);
                    self.output = GetRgbOutputBuffer(self.pixels)
                }
                QCMS_DATA_RGBA_8 => {
                    let (pixels, input) = GetBgraInputBuffer();
                    self.input = input;
                    self.pixels = pixels;
                    self.reference = GetRgbaOutputBuffer(self.pixels);
                    self.output = GetRgbaOutputBuffer(self.pixels);
                }
                QCMS_DATA_BGRA_8 => {
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
                QCMS_DATA_RGB_8 => ClearRgbBuffer(&mut self.output, self.pixels),
                QCMS_DATA_RGBA_8 | QCMS_DATA_BGRA_8 => {
                    ClearRgbaBuffer(&mut self.output, self.pixels)
                }
                _ => unreachable!("Unknown type!"),
            }
        }

        unsafe fn ProduceRef(&mut self, trans_fn: transform_fn_t) {
            trans_fn.unwrap()(
                self.transform,
                self.input.as_mut_ptr(),
                self.reference.as_mut_ptr(),
                self.pixels,
            )
        }

        fn CopyInputToRef(&mut self) {
            let pixelSize = match self.storage_type {
                QCMS_DATA_RGB_8 => 3,
                QCMS_DATA_RGBA_8 | QCMS_DATA_BGRA_8 => 4,
                _ => unreachable!("Unknown type!"),
            };
            self.reference
                .copy_from_slice(&self.input[..self.pixels * pixelSize])
        }

        unsafe fn ProduceOutput(&mut self, trans_fn: transform_fn_t) {
            self.ClearOutputBuffer();
            trans_fn.unwrap()(
                self.transform,
                self.input.as_mut_ptr(),
                self.output.as_mut_ptr(),
                self.pixels,
            )
        }

        unsafe fn VerifyOutput(&self, buf: &[u8]) -> bool {
            match self.storage_type {
                QCMS_DATA_RGB_8 => return CmpRgbBuffer(buf, &self.output, self.pixels),
                QCMS_DATA_RGBA_8 => return CmpRgbaBuffer(buf, &self.output, self.pixels),
                QCMS_DATA_BGRA_8 => return CmpBgraBuffer(buf, &self.output, self.pixels),
                _ => unreachable!("Unknown type!"),
            }
        }

        unsafe fn ProduceVerifyOutput(&mut self, trans_fn: transform_fn_t) -> bool {
            self.ProduceOutput(trans_fn);
            return self.VerifyOutput(&self.reference);
        }

        unsafe fn PrecacheOutput(&mut self) {
            qcms_profile_precache_output_transform(self.out_profile);
            self.precache = true;
        }
        unsafe fn TransformPrecache(&mut self) {
            assert_eq!(self.precache, false);
            assert!(self.SetBuffers(QCMS_DATA_RGB_8));
            assert!(self.SetTransformForType(QCMS_DATA_RGB_8));
            self.ProduceRef(Some(qcms_transform_data_rgb_out_lut));

            self.PrecacheOutput();
            assert!(self.SetTransformForType(QCMS_DATA_RGB_8));
            assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_precache)))
        }

        unsafe fn TransformPrecachePlatformExt(&mut self) {
            self.PrecacheOutput();

            // Verify RGB transforms.
            assert!(self.SetBuffers(QCMS_DATA_RGB_8));
            assert!(self.SetTransformForType(QCMS_DATA_RGB_8));
            self.ProduceRef(Some(qcms_transform_data_rgb_out_lut_precache));

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            {
                assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_sse2)));
                if is_x86_feature_detected!("avx") {
                    assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgb_out_lut_avx)))
                }
            }

            #[cfg(target_arch = "arm")]
            {
                if is_arm_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(qcms_transform_data_rgb_out_lut_neon))
                }
            }

            #[cfg(target_arch = "aarch64")]
            {
                if is_aarch64_feature_detected!("neon") {
                    assert!(self.ProduceVerifyOutput(qcms_transform_data_rgb_out_lut_neon))
                }
            }

            // Verify RGBA transform.
            assert!(self.SetBuffers(QCMS_DATA_RGBA_8));
            assert!(self.SetTransformForType(QCMS_DATA_RGBA_8));
            self.ProduceRef(Some(qcms_transform_data_rgba_out_lut_precache));

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            {
                assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_rgba_out_lut_sse2)));
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
            assert!(self.SetBuffers(QCMS_DATA_BGRA_8));
            assert!(self.SetTransformForType(QCMS_DATA_BGRA_8));
            self.ProduceRef(Some(qcms_transform_data_bgra_out_lut_precache));

            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            {
                assert!(self.ProduceVerifyOutput(Some(qcms_transform_data_bgra_out_lut_sse2)));
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
            pt.SetBuffers(QCMS_DATA_RGB_8);
            pt.SetTransformForType(QCMS_DATA_RGB_8);
            qcms_transform_data(
                pt.transform,
                pt.input.as_mut_ptr() as *mut c_void,
                pt.output.as_mut_ptr() as *mut c_void,
                pt.pixels,
            );
            assert!(pt.VerifyOutput(&pt.input));
            pt.TearDown();
        }
    }

    fn profile_from_path(file: &str) -> *mut qcms_profile {
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
}
