/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/Logging.h"
#include "mozilla/IntegerRange.h"

#include <functional>
#include <regex>
#include <string>

namespace mozilla {
namespace webgl {

static bool Contains(const std::string& str, const std::string& part) {
  return str.find(part) != size_t(-1);
}

/**
 * Narrow renderer string space down to representative replacements.
 * E.g. "GeForce RTX 3090" => "GeForce GTX 980"
 *
 * For example strings:
 * https://hackmd.io/Ductv3pQTMej74gbveD4yw
 */
static std::string ChooseDeviceReplacement(const std::string& str) {
  if (str.find("llvmpipe") == 0) return "llvmpipe";
  if (str.find("Apple") == 0) return "Apple M1";

  std::smatch m;

  // -

  if (Contains(str, "AMD ") || Contains(str, "FirePro") ||
      Contains(str, "Radeon")) {
    static const std::string RADEON_HD_3000 = "Radeon HD 3200 Graphics";
    static const std::string RADEON_HD_5850 = "Radeon HD 5850";
    static const std::string RADEON_R9_290 = "Radeon R9 200 Series";

    const auto RADEON_D3D_FL10_1 = RADEON_HD_3000;
    const auto RADEON_GCN_GEN2 = RADEON_R9_290;  // GCN Gen2

    if (Contains(str, "Vega")) {
      return RADEON_R9_290;
    }
    if (Contains(str, "VII")) {
      return RADEON_R9_290;
    }
    if (Contains(str, "Fury")) {
      return RADEON_R9_290;
    }
    static const std::regex kRadeon(
        "Radeon.*?((R[579X]|HD) )?([0-9][0-9][0-9]+)");
    if (std::regex_search(str, m, kRadeon)) {
      const auto& rxOrHd = m.str(2);
      const auto modelNum = stoul(m.str(3));
      if (rxOrHd == "HD") {
        if (modelNum >= 5000) {
          return RADEON_HD_5850;
        }
        if (modelNum >= 3000) {
          return RADEON_HD_3000;  // FL10_1
        }
        // HD 2000 is FL10_0, but webgl2 needs 10_1, so claim "old".
        return RADEON_D3D_FL10_1;
      }
      // R5/7/9/X
      return RADEON_R9_290;
    }
    static const std::regex kFirePro("FirePro.*?([VDW])[0-9][0-9][0-9]+");
    if (std::regex_search(str, m, kFirePro)) {
      const auto& vdw = m.str(1);
      if (vdw == "V") {
        return RADEON_HD_3000;  // FL10_1
      }
      return RADEON_R9_290;
    }

    if (Contains(str, "RENOIR")) {
      return RADEON_R9_290;
    }
    if (Contains(str, "ARUBA")) {
      return RADEON_HD_5850;
    }

    return RADEON_D3D_FL10_1;
  }

  // -

  static const std::string GEFORCE_8800 = "GeForce 8800 GTX";
  static const std::string GEFORCE_480 = "GeForce GTX 480";
  static const std::string GEFORCE_980 = "GeForce GTX 980";

  if (Contains(str, "NVIDIA") || Contains(str, "GeForce") ||
      Contains(str, "Quadro")) {
    auto ret = std::invoke([&]() {
      static const std::regex kGeForce("GeForce.*?([0-9][0-9][0-9]+)");
      if (std::regex_search(str, m, kGeForce)) {
        const auto modelNum = stoul(m.str(1));
        if (modelNum >= 8000) {
          // Tesla+: D3D10.0, SM4.0
          return GEFORCE_8800;
        }
        if (modelNum >= 900) {
          // Maxwell Gen2+: D3D12 FL12_1
          return GEFORCE_980;
        }
        if (modelNum >= 400) {
          // Fermi+: D3D12 FL11_0
          return GEFORCE_480;
        }
        // Tesla+: D3D10.0, SM4.0
        return GEFORCE_8800;
      }

      static const std::regex kQuadro("Quadro.*?([KMPVT]?)[0-9][0-9][0-9]+");
      if (std::regex_search(str, m, kQuadro)) {
        if (Contains(str, "RTX")) return GEFORCE_980;
        const auto archLetter = m.str(1);
        if (!archLetter.empty()) {
          switch (archLetter[0]) {
            case 'M':  // Maxwell
            case 'P':  // Pascal
            case 'V':  // Volta
            case 'T':  // Turing, mobile-only
              return GEFORCE_980;
            case 'K':  // Kepler
            default:
              return GEFORCE_480;
          }
        }
        return GEFORCE_8800;
      }

      /* Similarities for Titans:
       * 780
       * * GeForce GTX TITAN
       * * -
       * * Black
       * * Z
       * 980
       * * GeForce GTX TITAN X
       * 1080
       * * Nvidia TITAN X
       * * Nvidia TITAN Xp
       * * Nvidia TITAN V
       * 2080
       * * Nvidia TITAN RTX
       */
      static const std::regex kTitan("TITAN( [BZXVR])?");
      if (std::regex_search(str, m, kTitan)) {
        char letter = ' ';
        const auto sub = m.str(1);
        if (sub.length()) {
          letter = sub[1];
        }
        switch (letter) {
          case ' ':
          case 'B':
          case 'Z':
            return GEFORCE_480;
          default:
            return GEFORCE_980;
        }
      }
      // CI has str:"Tesla M60"
      if (Contains(str, "Tesla")) return GEFORCE_8800;

      return GEFORCE_8800;  // Unknown, but NV.
    });
    // On ANGLE: NVIDIA GeForce RTX 3070...
    // On WGL: GeForce RTX 3070...
    if (str.find("NVIDIA") == 0) {
      ret = "NVIDIA " + ret;
    }
    return ret;
  }

  static const std::regex kNouveau("NV(1?[0-9A-F][0-9A-F])");
  if (std::regex_match(str, m, kNouveau)) {
    const auto modelNum = stoul(m.str(1), nullptr, 16);
    // https://nouveau.freedesktop.org/CodeNames.html#NV110
    if (modelNum >= 0x120) return GEFORCE_980;
    if (modelNum >= 0xC0) return GEFORCE_480;
    return GEFORCE_8800;
  }

  // -

  if (Contains(str, "Intel")) {
    static const std::string HD_GRAPHICS = "Intel(R) HD Graphics";
    static const std::string HD_GRAPHICS_400 = "Intel(R) HD Graphics 400";
    static const std::string INTEL_945GM = "Intel 945GM";

    static const std::regex kIntelHD("Intel.*Graphics( P?([0-9][0-9][0-9]+))?");
    if (std::regex_search(str, m, kIntelHD)) {
      if (m.str(1).empty()) {
        return HD_GRAPHICS;
      }
      const auto modelNum = stoul(m.str(2));
      if (modelNum >= 5000) {
        return HD_GRAPHICS_400;
      }
      if (modelNum >= 1000) {
        return HD_GRAPHICS;
      }
      return HD_GRAPHICS_400;
    }

    return INTEL_945GM;
  }

  // -

  static const std::regex kAdreno("Adreno.*?([0-9][0-9][0-9]+)");
  if (std::regex_search(str, m, kAdreno)) {
    const auto modelNum = stoul(m.str(1));
    if (modelNum >= 600) {
      return "Adreno (TM) 650";
    }
    if (modelNum >= 500) {
      return "Adreno (TM) 540";
    }
    if (modelNum >= 400) {
      return "Adreno (TM) 430";
    }
    if (modelNum >= 300) {
      return "Adreno (TM) 330";
    }
    return "Adreno (TM) 225";
  }

  static const std::regex kMali("Mali.*?([0-9][0-9]+)");
  if (std::regex_search(str, m, kMali)) {
    const auto modelNum = stoul(m.str(1));
    if (modelNum >= 800) {
      return "Mali-T880";
    }
    if (modelNum >= 700) {
      return "Mali-T760";
    }
    if (modelNum >= 600) {
      return "Mali-T628";
    }
    if (modelNum >= 400) {
      return "Mali-400 MP";
    }
    return "Mali-G51";
  }

  if (Contains(str, "PowerVR")) {
    if (Contains(str, "Rogue")) {
      return "PowerVR Rogue G6200";
    }
    return "PowerVR SGX 540";
  }

  if (Contains(str, "Vivante")) return "Vivante GC1000";
  if (Contains(str, "VideoCore")) return "VideoCore IV HW";
  if (Contains(str, "Tegra")) return "NVIDIA Tegra";

  // -

  static const std::string D3D_WARP = "Microsoft Basic Render Driver";
  if (Contains(str, D3D_WARP)) return str;

  gfxCriticalNote << "Couldn't sanitize RENDERER device: " << str;
  return "Generic Renderer";
}

// -

std::string SanitizeRenderer(const std::string& str) {
  std::smatch m;

  // e.g. "ANGLE (AMD, AMD Radeon(TM) Graphics Direct3D11 vs_5_0 ps_5_0,
  // D3D11-27.20.1020.2002)"
  static const std::regex kReAngle(
      "ANGLE [(]([^,]*), ([^,]*)( Direct3D[^,]*), .*[)]");
  if (std::regex_match(str, m, kReAngle)) {
    const auto& vendor = m.str(1);
    const auto& renderer = m.str(2);
    const auto& d3d_suffix = m.str(3);

    const auto renderer2 = ChooseDeviceReplacement(renderer);
    return std::string("ANGLE (") + vendor + ", " + renderer2 + d3d_suffix +
           ")";
  } else if (Contains(str, "ANGLE")) {
    gfxCriticalError() << "Failed to parse ANGLE renderer: " << str;
  }

  static const std::regex kReOpenglEngine("(.*) OpenGL Engine");
  static const std::regex kRePcieSse2("(.*)(/PCIe?/SSE2)");
  static const std::regex kReStandard("(.*)( [(].*[)])");
  if (std::regex_match(str, m, kReOpenglEngine)) {
    const auto& dev = m.str(1);
    const auto dev2 = ChooseDeviceReplacement(dev);
    return dev2;
  }
  if (std::regex_match(str, m, kRePcieSse2)) {
    const auto& dev = m.str(1);
    const auto dev2 = ChooseDeviceReplacement(dev);
    return dev2 + m.str(2);
  }
  if (std::regex_match(str, m, kReStandard)) {
    const auto& dev = m.str(1);
    const auto dev2 = ChooseDeviceReplacement(dev);
    return dev2;
  }

  const auto& dev = str;
  const auto dev2 = ChooseDeviceReplacement(dev);
  return dev2;
}

};  // namespace webgl
};  // namespace mozilla
