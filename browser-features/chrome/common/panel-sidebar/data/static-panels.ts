/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function svgToDataUrl(svgContent: string): string {
  const svgBytes = new TextEncoder().encode(svgContent);
  let binString = "";
  svgBytes.forEach((byte) => {
    binString += String.fromCharCode(byte);
  });
  return `data:image/svg+xml;base64,${btoa(binString)}`;
}

const osIconRaw = import.meta.glob("../icons/os.svg", {
  query: "?raw",
  import: "default",
  eager: true,
}) as { [key: string]: string };

const osIcon = svgToDataUrl(Object.values(osIconRaw)[0]);

export type StaticPanelConfig = {
  url: string;
  icon: string;
  l10n: string;
  defaultWidth: number;
};

export type StaticPanelData = Record<string, StaticPanelConfig>;

/**
 * 基本的な静的パネルデータ
 */
const BASE_STATIC_PANEL_DATA: StaticPanelData = {
  "floorp//bmt": {
    url: "chrome://browser/content/places/places.xhtml",
    icon: "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxNiIgaGVpZ2h0PSIxNiIgdmlld0JveD0iMCAwIDE2IDE2IiBmaWxsPSJjb250ZXh0LWZpbGwiIGZpbGwtb3BhY2l0eT0iY29udGV4dC1maWxsLW9wYWNpdHkiPjxwYXRoIGZpbGwtcnVsZT0iZXZlbm9kZCIgZD0iTTEgNmEyIDIgMCAwIDEgMi0yaDFWM2EyIDIgMCAwIDEgMi0yaDRhMiAyIDAgMCAxIDIgMnYxaDFhMiAyIDAgMCAxIDIgMnY3YTIgMiAwIDAgMS0yIDJIM2EyIDIgMCAwIDEtMi0yVjZabTMtLjc1SDNhLjc1Ljc1IDAgMCAwLS43NS43NXYySDF2LS4zNzVhLjYyNS42MjUgMCAxIDEgMS4yNSAwVjhoNS41di0uMzc1YS42MjUuNjI1IDAgMSAxIDEuMjUgMFY4aDEuNzVWNmEuNzUuNzUgMCAwIDAtLjc1LS43NUg0Wk0yLjI1IDEzYzAgLjQxNC4zMzYuNzUuNzUuNzVoMTBhLjc1Ljc1IDAgMCAwIC43NS0uNzVWOS4yNUgxMnYxLjEyNWEuNjI1LjYyNSAwIDEgMS0xLjI1IDBWOS4yNWgtNS41djEuMTI1YS42MjUuNjI1IDAgMSAxLTEuMjUgMFY5LjI1SDIuMjVWMTNaTTEwIDIuMjVINmEuNzUuNzUgMCAwIDAtLjc1Ljc1djFoNS41VjNhLjc1Ljc1IDAgMCAwLS43NS0uNzVaIiBjbGlwLXJ1bGU9ImV2ZW5vZGQiLz48L3N2Zz4=",
    l10n: "browser-manager-sidebar",
    defaultWidth: 600,
  },

  "floorp//bookmarks": {
    url: "chrome://browser/content/places/bookmarksSidebar.xhtml",
    icon: "chrome://browser/skin/bookmark.svg",
    l10n: "bookmark-sidebar",
    defaultWidth: 415,
  },

  "floorp//history": {
    url: "chrome://browser/content/places/historySidebar.xhtml",
    icon: "chrome://browser/skin/history.svg",
    l10n: "history-sidebar",
    defaultWidth: 415,
  },

  "floorp//downloads": {
    url: "about:downloads",
    icon: "chrome://browser/skin/downloads/downloads.svg",
    l10n: "download-sidebar",
    defaultWidth: 415,
  },

  "floorp//notes": {
    url: import.meta.env.DEV
      ? "http://localhost:5188"
      : "chrome://noraneko-notes/content/index.html",
    icon: "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxNiIgaGVpZ2h0PSIxNiIgdmlld0JveD0iMCAwIDE2IDE2IiBmaWxsPSJjb250ZXh0LWZpbGwiPgogIDxwYXRoIGQ9Ik0xNC4zNTQsMi4zNTMgTDEzLjY0NiwxLjY0NiBDMTIuODYzNDEzNSwwLjg2OTA3NzIwMyAxMS42MDA1ODY1LDAuODY5MDc3MjAzIDEwLjgxOCwxLjY0NiBMMTAuNDM5LDIuMDI1IEMxMC4yNDM4MDksMi4yMjAyNDk5MyAxMC4yNDM4MDksMi41MzY3NTAwNyAxMC40MzksMi43MzIgTDEzLjI2OCw1LjU2MSBDMTMuNDYzMjQ5OSw1Ljc1NjE5MDk3IDEzLjc3OTc1MDEsNS43NTYxOTA5NyAxMy45NzUsNS41NjEgTDE0LjM1NCw1LjE4MiBDMTUuMTMxMDM5Miw0LjM5OTA3MTU2IDE1LjEzMTAzOTIsMy4xMzU5Mjg0NCAxNC4zNTQsMi4zNTMgTDE0LjM1NCwyLjM1MyBaIE05LjczMiwzLjQzOSBDOS41MzY3NTAwNywzLjI0MzgwOTAzIDkuMjIwMjQ5OTMsMy4yNDM4MDkwMyA5LjAyNSwzLjQzOSBMMy4yNDYsOS4yMTggQzMuMDQ2MDk3ODgsOS40MjAyMzcyIDIuODkxOTU2MjYsOS42NjMwNDQzNiAyLjc5NCw5LjkzIEwxLjAzOCwxNC4zMiBDMC45Nzg5Mzc5NjgsMTQuNDczMDQxOCAwLjk5ODcwMzY2OCwxNC42NDUzMiAxLjA5MDg5MjExLDE0Ljc4MTAwODYgQzEuMTgzMDgwNTQsMTQuOTE2Njk3MiAxLjMzNTk2MzU1LDE0Ljk5ODUzNCAxLjUsMTUgQzEuNTY0NDY1OTMsMTQuOTk5OTAxNiAxLjYyODMwNDU1LDE0Ljk4NzMzNzYgMS42ODgsMTQuOTYzIEw2LjA3LDEzLjIxMSBDNi4zMzg4NDQ2MSwxMy4xMTM1NDA2IDYuNTgzMTkxOTgsMTIuOTU4NjA1MiA2Ljc4NiwxMi43NTcgTDEyLjU2NSw2Ljk3OSBDMTIuNzYwMTkxLDYuNzgzNzUwMDcgMTIuNzYwMTkxLDYuNDY3MjQ5OTMgMTIuNTY1LDYuMjcyIEw5LjczMiwzLjQzOSBaIE01LjE2MSwxMi41IEwyLjYxMiwxMy41MiBDMi41NzQ4NTM4MywxMy41MzQ4Njg3IDIuNTMyNDIwNTIsMTMuNTI2MTY0MiAyLjUwNDEyODE0LDEzLjQ5Nzg3MTkgQzIuNDc1ODM1NzcsMTMuNDY5NTc5NSAyLjQ2NzEzMTI3LDEzLjQyNzE0NjIgMi40ODIsMTMuMzkgTDMuNSwxMC44MzEgQzMuNTEzNDAwNjIsMTAuODAxNTQgMy41NDAyMzAzMiwxMC43ODAzODcgMy41NzIwMDQxLDEwLjc3NDIzMDggQzMuNjAzNzc3ODcsMTAuNzY4MDc0NiAzLjYzNjU2NjMzLDEwLjc3NzY3NjYgMy42NiwxMC44IEw1LjIsMTIuMzM1IEM1LjIyNDIyNTgxLDEyLjM1OTUwODggNS4yMzQxMjUzMiwxMi4zOTQ3NjQ0IDUuMjI2MTk4MzgsMTIuNDI4MzAxNCBDNS4yMTgyNzE0MywxMi40NjE4Mzg1IDUuMTkzNjM1MSwxMi40ODg5MzEgNS4xNjEsMTIuNSBMNS4xNjEsMTIuNSBaIi8+Cjwvc3ZnPg==",
    l10n: "notes-sidebar",
    defaultWidth: 550,
  },
};

/**
 * 条件に応じて追加のパネルを返す関数
 * ここで条件をチェックして、必要に応じてパネルを追加できます
 *
 * 例:
 * - 実験機能の有効状態をチェック
 * - 環境変数や設定をチェック
 * - 拡張機能の有無をチェック
 *
 * @returns 追加するパネルのオブジェクト（空の場合は空オブジェクト）
 */
function getConditionalPanels(): StaticPanelData {
  const conditionalPanels: StaticPanelData = {};

  // Floorp OS が有効な場合にパネルを追加
  try {
    const floorpOSEnabled = Services.prefs.getBoolPref(
      "floorp.os.enabled",
      false,
    );
    if (floorpOSEnabled) {
      conditionalPanels["floorp//floorp-os"] = {
        url: `http://localhost:8081`,
        icon: osIcon,
        l10n: "floorp-os-sidebar",
        defaultWidth: 600,
      };
    }
  } catch (_e) {
    console.error("Failed to get conditional panels:", _e);
  }

  return conditionalPanels;
}

/**
 * 静的パネルデータを取得します
 * 条件に応じて追加のパネルが含まれます
 */
export const STATIC_PANEL_DATA: StaticPanelData = {
  ...BASE_STATIC_PANEL_DATA,
  ...getConditionalPanels(),
};
