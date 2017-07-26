// |jit-test| --ion-limit-script-size=off; error:ReferenceError

function f() {
    for (var i = 0; i < 1; i++) {
        let x00, x01, x02, x03, x04, x05, x06, x07, x08, x09, x0A, x0B, x0C, x0D, x0E, x0F,
            x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x1A, x1B, x1C, x1D, x1E, x1F,
            x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x2A, x2B, x2C, x2D, x2E, x2F,
            x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x3A, x3B, x3C, x3D, x3E, x3F,
            x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x4A, x4B, x4C, x4D, x4E, x4F,
            x50, x51, x52, x53, x54, x55, x56, x57, x58, x59, x5A, x5B, x5C, x5D, x5E, x5F,
            x60, x61, x62, x63, x64, x65, x66, x67, x68, x69, x6A, x6B, x6C, x6D, x6E, x6F,
            x70, x71, x72, x73, x74, x75, x76, x77, x78, x79, x7A, x7B, x7C, x7D, x7E, x7F,
            x80, x81, x82, x83, x84, x85, x86, x87, x88, x89, x8A, x8B, x8C, x8D, x8E, x8F,
            x90, x91, x92, x93, x94, x95, x96, x97, x98, x99, x9A, x9B, x9C, x9D, x9E, x9F,
            xA0, xA1, xA2, xA3, xA4, xA5, xA6, xA7, xA8, xA9, xAA, xAB, xAC, xAD, xAE, xAF,
            xB0, xB1, xB2, xB3, xB4, xB5, xB6, xB7, xB8, xB9, xBA, xBB, xBC, xBD, xBE, xBF,
            xC0, xC1, xC2, xC3, xC4, xC5, xC6, xC7, xC8, xC9, xCA, xCB, xCC, xCD, xCE, xCF,
            xD0, xD1, xD2, xD3, xD4, xD5, xD6, xD7, xD8, xD9, xDA, xDB, xDC, xDD, xDE, xDF,
            xE0, xE1, xE2, xE3, xE4, xE5, xE6, xE7, xE8, xE9, xEA, xEB, xEC, xED, xEE, xEF,
            xF0, xF1, xF2, xF3, xF4, xF5, xF6, xF7, xF8, xF9, xFA, xFB, xFC, xFD, xFE, xFF;
        let y00, y01, y02, y03, y04, y05, y06, y07, y08, y09, y0A, y0B, y0C, y0D, y0E, y0F,
            y10, y11, y12, y13, y14, y15, y16, y17, y18, y19, y1A, y1B, y1C, y1D, y1E, y1F,
            y20, y21, y22, y23, y24, y25, y26, y27, y28, y29, y2A, y2B, y2C, y2D, y2E, y2F,
            y30, y31, y32, y33, y34, y35, y36, y37, y38, y39, y3A, y3B, y3C, y3D, y3E, y3F,
            y40, y41, y42, y43, y44, y45, y46, y47, y48, y49, y4A, y4B, y4C, y4D, y4E, y4F,
            y50, y51, y52, y53, y54, y55, y56, y57, y58, y59, y5A, y5B, y5C, y5D, y5E, y5F,
            y60, y61, y62, y63, y64, y65, y66, y67, y68, y69, y6A, y6B, y6C, y6D, y6E, y6F,
            y70, y71, y72, y73, y74, y75, y76, y77, y78, y79, y7A, y7B, y7C, y7D, y7E, y7F,
            y80, y81, y82, y83, y84, y85, y86, y87, y88, y89, y8A, y8B, y8C, y8D, y8E, y8F,
            y90, y91, y92, y93, y94, y95, y96, y97, y98, y99, y9A, y9B, y9C, y9D, y9E, y9F,
            yA0, yA1, yA2, yA3, yA4, yA5, yA6, yA7, yA8, yA9, yAA, yAB, yAC, yAD, yAE, yAF,
            yB0, yB1, yB2, yB3, yB4, yB5, yB6, yB7, yB8, yB9, yBA, yBB, yBC, yBD, yBE, yBF,
            yC0, yC1, yC2, yC3, yC4, yC5, yC6, yC7, yC8, yC9, yCA, yCB, yCC, yCD, yCE, yCF,
            yD0, yD1, yD2, yD3, yD4, yD5, yD6, yD7, yD8, yD9, yDA, yDB, yDC, yDD, yDE, yDF,
            yE0, yE1, yE2, yE3, yE4, yE5, yE6, yE7, yE8, yE9, yEA, yEB, yEC, yED, yEE, yEF,
            yF0, yF1, yF2, yF3, yF4, yF5, yF6, yF7, yF8, yF9, yFA, yFB, yFC, yFD, yFE, yFF;

        if (b()) {
            x00 = x01 = x02 = x03 = x04 = x05 = x06 = x07 = a();
            x08 = x09 = x0A = x0B = x0C = x0D = x0E = x0F = a();
            x10 = x11 = x12 = x13 = x14 = x15 = x16 = x17 = a();
            x18 = x19 = x1A = x1B = x1C = x1D = x1E = x1F = a();
            x20 = x21 = x22 = x23 = x24 = x25 = x26 = x27 = a();
            x28 = x29 = x2A = x2B = x2C = x2D = x2E = x2F = a();
            x30 = x31 = x32 = x33 = x34 = x35 = x36 = x37 = a();
            x38 = x39 = x3A = x3B = x3C = x3D = x3E = x3F = a();
            x40 = x41 = x42 = x43 = x44 = x45 = x46 = x47 = a();
            x48 = x49 = x4A = x4B = x4C = x4D = x4E = x4F = a();
            x50 = x51 = x52 = x53 = x54 = x55 = x56 = x57 = a();
            x58 = x59 = x5A = x5B = x5C = x5D = x5E = x5F = a();
            x60 = x61 = x62 = x63 = x64 = x65 = x66 = x67 = a();
            x68 = x69 = x6A = x6B = x6C = x6D = x6E = x6F = a();
            x70 = x71 = x72 = x73 = x74 = x75 = x76 = x77 = a();
            x78 = x79 = x7A = x7B = x7C = x7D = x7E = x7F = a();
            x80 = x81 = x82 = x83 = x84 = x85 = x86 = x87 = a();
            x88 = x89 = x8A = x8B = x8C = x8D = x8E = x8F = a();
            x90 = x91 = x92 = x93 = x94 = x95 = x96 = x97 = a();
            x98 = x99 = x9A = x9B = x9C = x9D = x9E = x9F = a();
            xA0 = xA1 = xA2 = xA3 = xA4 = xA5 = xA6 = xA7 = a();
            xA8 = xA9 = xAA = xAB = xAC = xAD = xAE = xAF = a();
            xB0 = xB1 = xB2 = xB3 = xB4 = xB5 = xB6 = xB7 = a();
            xB8 = xB9 = xBA = xBB = xBC = xBD = xBE = xBF = a();
            xC0 = xC1 = xC2 = xC3 = xC4 = xC5 = xC6 = xC7 = a();
            xC8 = xC9 = xCA = xCB = xCC = xCD = xCE = xCF = a();
            xD0 = xD1 = xD2 = xD3 = xD4 = xD5 = xD6 = xD7 = a();
            xD8 = xD9 = xDA = xDB = xDC = xDD = xDE = xDF = a();
            xE0 = xE1 = xE2 = xE3 = xE4 = xE5 = xE6 = xE7 = a();
            xE8 = xE9 = xEA = xEB = xEC = xED = xEE = xEF = a();
            xF0 = xF1 = xF2 = xF3 = xF4 = xF5 = xF6 = xF7 = a();
            xF8 = xF9 = xFA = xFB = xFC = xFD = xFE = xFF = a();
        }

        foo(x00, x01, x02, x03, x04, x05, x06, x07, x08, x09, x0A, x0B, x0C, x0D, x0E, x0F,
            x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x1A, x1B, x1C, x1D, x1E, x1F,
            x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x2A, x2B, x2C, x2D, x2E, x2F,
            x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x3A, x3B, x3C, x3D, x3E, x3F,
            x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x4A, x4B, x4C, x4D, x4E, x4F,
            x50, x51, x52, x53, x54, x55, x56, x57, x58, x59, x5A, x5B, x5C, x5D, x5E, x5F,
            x60, x61, x62, x63, x64, x65, x66, x67, x68, x69, x6A, x6B, x6C, x6D, x6E, x6F,
            x70, x71, x72, x73, x74, x75, x76, x77, x78, x79, x7A, x7B, x7C, x7D, x7E, x7F,
            x80, x81, x82, x83, x84, x85, x86, x87, x88, x89, x8A, x8B, x8C, x8D, x8E, x8F,
            x90, x91, x92, x93, x94, x95, x96, x97, x98, x99, x9A, x9B, x9C, x9D, x9E, x9F,
            xA0, xA1, xA2, xA3, xA4, xA5, xA6, xA7, xA8, xA9, xAA, xAB, xAC, xAD, xAE, xAF,
            xB0, xB1, xB2, xB3, xB4, xB5, xB6, xB7, xB8, xB9, xBA, xBB, xBC, xBD, xBE, xBF,
            xC0, xC1, xC2, xC3, xC4, xC5, xC6, xC7, xC8, xC9, xCA, xCB, xCC, xCD, xCE, xCF,
            xD0, xD1, xD2, xD3, xD4, xD5, xD6, xD7, xD8, xD9, xDA, xDB, xDC, xDD, xDE, xDF,
            xE0, xE1, xE2, xE3, xE4, xE5, xE6, xE7, xE8, xE9, xEA, xEB, xEC, xED, xEE, xEF,
            xF0, xF1, xF2, xF3, xF4, xF5, xF6, xF7, xF8, xF9, xFA, xFB, xFC, xFD, xFE, xFF);
    }
}

f();
