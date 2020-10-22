#[cfg(test)]
mod test {
    use crate::{
        iccread::*, transform::*, transform_util::lut_inverse_interp16, QCMS_INTENT_PERCEPTUAL,
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
        unsafe {
            lut_inverse_interp16(5, lutTable1.as_mut_ptr(), lutTable1.len() as i32);
            lut_inverse_interp16(5, lutTable2.as_mut_ptr(), lutTable2.len() as i32);
        }
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
            unsafe { lut_inverse_interp16(i, lutTable.as_mut_ptr(), lutTable.len() as i32) };
        }

        // Lookup the interesting points

        value = unsafe { lut_inverse_interp16(0, lutTable.as_mut_ptr(), lutTable.len() as i32) };
        assert!(value <= 20 * 256);

        value = unsafe { lut_inverse_interp16(1, lutTable.as_mut_ptr(), lutTable.len() as i32) };
        assert!(value > 20 * 256);

        value =
            unsafe { lut_inverse_interp16(65535, lutTable.as_mut_ptr(), lutTable.len() as i32) };
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
            unsafe {
                lut_inverse_interp16(i, lutTable.as_mut_ptr(), lutTable.len() as i32);
            }
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
                sRGB_profile,
                QCMS_DATA_RGB_8,
                other,
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
                other,
                QCMS_DATA_GRAYA_8,
                sRGB_profile,
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
                profile,
                QCMS_DATA_RGB_8,
                srgb_profile,
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
}
