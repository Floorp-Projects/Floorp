/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Constants for NRWebScraperChild
 */

import type { HighlightOptionsInput } from "./types.ts";

/**
 * Minimum duration (ms) that a highlight must be visible before cleanup
 */
export const MINIMUM_HIGHLIGHT_DURATION = 500;

/**
 * Preset configurations for different highlight actions
 */
export const HIGHLIGHT_PRESETS: Record<string, HighlightOptionsInput> = {
  Highlight: {
    duration: 1800,
    focus: true,
    scrollBehavior: "smooth",
    padding: 14,
    delay: 400,
  },
  Inspect: {
    duration: 1800,
    focus: false,
    scrollBehavior: "smooth",
    padding: 16,
    delay: 400,
  },
  InspectPeek: {
    duration: 1800,
    focus: false,
    scrollBehavior: "smooth",
    padding: 14,
    delay: 1200,
  },
  Input: {
    duration: 1800,
    focus: true,
    scrollBehavior: "smooth",
    padding: 14,
    delay: 400,
  },
  Click: {
    duration: 2000,
    focus: true,
    scrollBehavior: "smooth",
    padding: 12,
    delay: 500,
  },
  Fill: {
    duration: 1500,
    focus: true,
    scrollBehavior: "smooth",
    padding: 18,
    delay: 500,
  },
  Submit: {
    duration: 1800,
    focus: true,
    scrollBehavior: "smooth",
    padding: 12,
    delay: 500,
  },
};

/**
 * Lucide icon SVG path data mapping
 * These are the SVG path strings from Lucide icons
 */
export const LUCIDE_ICON_PATHS: Record<string, string> = {
  Eye: "M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6z",
  Pencil:
    "M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7 M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z",
  MousePointerClick:
    "M9 9l5 12 1.774-5.226L21 14 9 9z M16.071 16.071l4.243 4.243 M7.188 2.239l.777 2.897M5.136 7.965l-2.898-.777M13.95 4.05l-2.122 2.122m-5.657 5.656l-2.12 2.122",
  Send: "M22 2L11 13 M22 2l-7 20-4-9-9-4z",
  Zap: "M13 2L3 14h9l-1 8 10-12h-9l1-8z",
};

/**
 * Fallback translation strings (English)
 */
export const FALLBACK_TRANSLATIONS: Record<string, string> = {
  panelTitle: "Floorp OS is operating",
  panelDefaultMessage: "{{action}} in progress...",
  panelProgressSummary: "Affects {{count}} element(s)",
  formSummary: "Filling form: {{count}} field(s)",
  formFieldProgress: 'Field {{current}}/{{total}}: "{{value}}"',
  inputValueSet: 'Set input value: "{{value}}"',
  clickElementWithText: 'Click {{tag}}: "{{text}}"',
  clickElementNoText: "Clicking {{tag}} element",
  inspectPageHtml: "Retrieved full page HTML",
  inspectGetElement: "Retrieved element: {{selector}}",
  inspectGetElements: "{{count}} element(s) matched: {{selector}}",
  inspectGetElementByText: 'Retrieved element matching text: "{{text}}"',
  inspectGetElementText: "Retrieved text from: {{selector}}",
  inspectGetValue: "Retrieved value: {{selector}}",
  submitForm: "Submitting form: {{name}}",
  actionLabelHighlight: "Highlight",
  actionLabelInspect: "Inspect",
  actionLabelFill: "Fill",
  actionLabelInput: "Input",
  actionLabelClick: "Click",
  actionLabelSubmit: "Submit",
};

/**
 * CSS styles for highlight overlays and panels
 */
export const HIGHLIGHT_STYLES = `@keyframes nr-webscraper-highlight-pulse {
  0% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-45),
                0 0 20px var(--nr-highlight-color-alpha-25);
    transform: scale(1);
  }
  50% {
    box-shadow: 0 0 0 12px var(--nr-highlight-color-alpha-25),
                0 0 40px var(--nr-highlight-color-alpha-45);
    transform: scale(1.02);
  }
  100% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-0),
                0 0 20px var(--nr-highlight-color-alpha-25);
    transform: scale(1);
  }
}

@keyframes nr-webscraper-highlight-glow {
  0%, 100% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-45),
                0 0 20px var(--nr-highlight-color-alpha-25),
                inset 0 0 20px var(--nr-highlight-color-alpha-15);
  }
  50% {
    box-shadow: 0 0 0 8px var(--nr-highlight-color-alpha-30),
                0 0 50px var(--nr-highlight-color-alpha-50),
                inset 0 0 30px var(--nr-highlight-color-alpha-20);
  }
}

@keyframes nr-webscraper-info-slide-in {
  from {
    opacity: 0;
    transform: translateY(-20px) scale(0.95);
  }
  to {
    opacity: 1;
    transform: translateY(0) scale(1);
  }
}

@keyframes nr-webscraper-progress {
  from {
    width: 0%;
  }
  to {
    width: 100%;
  }
}

.nr-webscraper-highlight-overlay {
  --nr-highlight-color-alpha-0: rgba(59, 130, 246, 0);
  --nr-highlight-color-alpha-15: rgba(59, 130, 246, 0.15);
  --nr-highlight-color-alpha-20: rgba(59, 130, 246, 0.20);
  --nr-highlight-color-alpha-25: rgba(59, 130, 246, 0.25);
  --nr-highlight-color-alpha-30: rgba(59, 130, 246, 0.30);
  --nr-highlight-color-alpha-45: rgba(59, 130, 246, 0.45);
  --nr-highlight-color-alpha-50: rgba(59, 130, 246, 0.50);
  --nr-highlight-color: rgba(59, 130, 246, 0.95);
  --nr-highlight-bg: rgba(59, 130, 246, 0.12);
  --nr-label-bg: rgba(37, 99, 235, 0.94);
  position: fixed;
  border-radius: 12px;
  border: 3px solid var(--nr-highlight-color);
  box-shadow: 0 0 0 6px var(--nr-highlight-color-alpha-25),
              0 0 30px var(--nr-highlight-color-alpha-30);
  pointer-events: none;
  z-index: 2147483640;
  opacity: 0;
  transform: scale(0.96);
  transition: opacity 150ms ease-out, transform 150ms ease-out;
  background: var(--nr-highlight-bg);
  animation: nr-webscraper-highlight-glow 2s ease-in-out infinite;
}

.nr-webscraper-highlight-overlay--read {
  --nr-highlight-color-alpha-0: rgba(34, 197, 94, 0);
  --nr-highlight-color-alpha-15: rgba(34, 197, 94, 0.15);
  --nr-highlight-color-alpha-20: rgba(34, 197, 94, 0.20);
  --nr-highlight-color-alpha-25: rgba(34, 197, 94, 0.25);
  --nr-highlight-color-alpha-30: rgba(34, 197, 94, 0.30);
  --nr-highlight-color-alpha-45: rgba(34, 197, 94, 0.45);
  --nr-highlight-color-alpha-50: rgba(34, 197, 94, 0.50);
  --nr-highlight-color: rgba(34, 197, 94, 0.95);
  --nr-highlight-bg: rgba(34, 197, 94, 0.12);
  --nr-label-bg: rgba(22, 163, 74, 0.94);
}

.nr-webscraper-highlight-overlay--write {
  --nr-highlight-color-alpha-0: rgba(168, 85, 247, 0);
  --nr-highlight-color-alpha-15: rgba(168, 85, 247, 0.15);
  --nr-highlight-color-alpha-20: rgba(168, 85, 247, 0.20);
  --nr-highlight-color-alpha-25: rgba(168, 85, 247, 0.25);
  --nr-highlight-color-alpha-30: rgba(168, 85, 247, 0.30);
  --nr-highlight-color-alpha-45: rgba(168, 85, 247, 0.45);
  --nr-highlight-color-alpha-50: rgba(168, 85, 247, 0.50);
  --nr-highlight-color: rgba(168, 85, 247, 0.95);
  --nr-highlight-bg: rgba(168, 85, 247, 0.12);
  --nr-label-bg: rgba(147, 51, 234, 0.94);
}

.nr-webscraper-highlight-overlay--click {
  --nr-highlight-color-alpha-0: rgba(249, 115, 22, 0);
  --nr-highlight-color-alpha-15: rgba(249, 115, 22, 0.15);
  --nr-highlight-color-alpha-20: rgba(249, 115, 22, 0.20);
  --nr-highlight-color-alpha-25: rgba(249, 115, 22, 0.25);
  --nr-highlight-color-alpha-30: rgba(249, 115, 22, 0.30);
  --nr-highlight-color-alpha-45: rgba(249, 115, 22, 0.45);
  --nr-highlight-color-alpha-50: rgba(249, 115, 22, 0.50);
  --nr-highlight-color: rgba(249, 115, 22, 0.95);
  --nr-highlight-bg: rgba(249, 115, 22, 0.12);
  --nr-label-bg: rgba(234, 88, 12, 0.94);
}

.nr-webscraper-highlight-overlay--submit {
  --nr-highlight-color-alpha-0: rgba(239, 68, 68, 0);
  --nr-highlight-color-alpha-15: rgba(239, 68, 68, 0.15);
  --nr-highlight-color-alpha-20: rgba(239, 68, 68, 0.20);
  --nr-highlight-color-alpha-25: rgba(239, 68, 68, 0.25);
  --nr-highlight-color-alpha-30: rgba(239, 68, 68, 0.30);
  --nr-highlight-color-alpha-45: rgba(239, 68, 68, 0.45);
  --nr-highlight-color-alpha-50: rgba(239, 68, 68, 0.50);
  --nr-highlight-color: rgba(239, 68, 68, 0.95);
  --nr-highlight-bg: rgba(239, 68, 68, 0.12);
  --nr-label-bg: rgba(220, 38, 38, 0.94);
}

.nr-webscraper-highlight-overlay.nr-webscraper-highlight-overlay--visible {
  opacity: 1;
  transform: scale(1);
  animation: nr-webscraper-highlight-pulse 1200ms ease-in-out 1,
             nr-webscraper-highlight-glow 2s ease-in-out infinite;
  transition: opacity 300ms ease-out, transform 300ms ease-out;
}

.nr-webscraper-highlight-label {
  position: absolute;
  top: -32px;
  left: 0;
  transform: translateY(-4px);
  background: var(--nr-label-bg);
  color: #fff;
  padding: 6px 12px;
  border-radius: 9999px;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 13px;
  font-weight: 700;
  white-space: nowrap;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3),
              0 0 20px var(--nr-highlight-color-alpha-50);
  letter-spacing: 0.3px;
  text-transform: uppercase;
}

.nr-webscraper-highlight-overlay.nr-webscraper-highlight-overlay--below .nr-webscraper-highlight-label {
  top: auto;
  bottom: -28px;
  transform: translateY(4px);
}

.nr-webscraper-info-panel {
  /* CSS変数のデフォルト値を定義（overlayクラスがない場合のフォールバック） */
  --nr-highlight-color-alpha-50: rgba(59, 130, 246, 0.50);
  --nr-highlight-color: rgba(59, 130, 246, 0.95);
  --nr-label-bg: rgba(37, 99, 235, 0.94);
  position: fixed;
  top: 16px;
  right: 16px;
  max-width: 420px;
  min-width: 320px;
  background: rgba(17, 24, 39, 0.98);
  backdrop-filter: blur(16px);
  border: 2px solid rgba(255, 255, 255, 0.15);
  border-radius: 16px;
  box-shadow: 0 25px 60px rgba(0, 0, 0, 0.4),
              0 0 0 1px rgba(255, 255, 255, 0.08),
              inset 0 1px 0 rgba(255, 255, 255, 0.1);
  pointer-events: none;
  z-index: 2147483642 !important;
  animation: nr-webscraper-info-slide-in 300ms cubic-bezier(0.34, 1.56, 0.64, 1);
  color: #fff;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 13px;
  padding: 18px 20px;
}

.nr-webscraper-info-panel__title {
  font-weight: 700;
  font-size: 15px;
  margin-bottom: 10px;
  display: flex;
  align-items: center;
  gap: 10px;
  color: #fff;
}

.nr-webscraper-info-panel__icon {
  width: 20px;
  height: 20px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  line-height: 1;
}

.nr-webscraper-info-panel__badge {
  display: inline-flex;
  align-items: center;
  padding: 4px 10px;
  border-radius: 9999px;
  font-size: 11px;
  font-weight: 700;
  background: var(--nr-label-bg);
  color: #fff;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
}

.nr-webscraper-info-panel__content {
  color: rgba(255, 255, 255, 0.9);
  line-height: 1.6;
  margin-bottom: 8px;
}

.nr-webscraper-info-panel__progress {
  margin-top: 12px;
  height: 4px;
  background: rgba(255, 255, 255, 0.1);
  border-radius: 2px;
  overflow: hidden;
  position: relative;
}

.nr-webscraper-info-panel__progress-bar {
  height: 100%;
  background: linear-gradient(90deg, var(--nr-label-bg), var(--nr-highlight-color));
  border-radius: 2px;
  animation: nr-webscraper-progress 0.3s ease-out;
  box-shadow: 0 0 10px var(--nr-highlight-color-alpha-50);
}

.nr-webscraper-info-panel__selector {
  margin-top: 10px;
  padding: 10px 12px;
  background: rgba(0, 0, 0, 0.4);
  border-radius: 8px;
  font-family: "SF Mono", Monaco, Consolas, monospace;
  font-size: 11px;
  color: rgba(168, 85, 247, 0.95);
  word-break: break-all;
  border: 1px solid rgba(255, 255, 255, 0.08);
  line-height: 1.4;
}

.nr-webscraper-info-panel__count {
  margin-top: 8px;
  font-size: 12px;
  color: rgba(255, 255, 255, 0.7);
  font-weight: 500;
  display: flex;
  align-items: center;
  gap: 6px;
}

/* InfoPanel用のカラークラス（オーバーレイと同じカラーテーマを適用） */
.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--read {
  --nr-highlight-color-alpha-50: rgba(34, 197, 94, 0.50);
  --nr-highlight-color: rgba(34, 197, 94, 0.95);
  --nr-label-bg: rgba(22, 163, 74, 0.94);
}

.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--write {
  --nr-highlight-color-alpha-50: rgba(168, 85, 247, 0.50);
  --nr-highlight-color: rgba(168, 85, 247, 0.95);
  --nr-label-bg: rgba(147, 51, 234, 0.94);
}

.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--click {
  --nr-highlight-color-alpha-50: rgba(249, 115, 22, 0.50);
  --nr-highlight-color: rgba(249, 115, 22, 0.95);
  --nr-label-bg: rgba(234, 88, 12, 0.94);
}

.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--submit {
  --nr-highlight-color-alpha-50: rgba(239, 68, 68, 0.50);
  --nr-highlight-color: rgba(239, 68, 68, 0.95);
  --nr-label-bg: rgba(220, 38, 38, 0.94);
}

/* AI Control Overlay - blocks user interaction during automation */
@keyframes nr-webscraper-control-pulse {
  0%, 100% {
    border-color: rgba(59, 130, 246, 0.5);
    box-shadow: inset 0 0 60px rgba(59, 130, 246, 0.08),
                0 0 0 4px rgba(59, 130, 246, 0.15);
  }
  50% {
    border-color: rgba(59, 130, 246, 0.8);
    box-shadow: inset 0 0 100px rgba(59, 130, 246, 0.15),
                0 0 0 8px rgba(59, 130, 246, 0.25);
  }
}

.nr-webscraper-control-overlay {
  position: fixed !important;
  inset: 0 !important;
  z-index: 2147483645 !important;
  background: radial-gradient(
    circle at center,
    transparent 0%,
    transparent 70%,
    rgba(59, 130, 246, 0.4) 90%,
    rgba(59, 130, 246, 0.6) 100%
  ) !important;
  pointer-events: all !important;
  cursor: not-allowed !important;
  overflow: hidden !important;
  /* Additional blocking properties */
  user-select: none !important;
  -webkit-user-select: none !important;
  -moz-user-select: none !important;
  touch-action: none !important;
  -webkit-touch-callout: none !important;
}

.nr-webscraper-control-overlay__label {
  position: fixed;
  top: 16px;
  left: 16px;
  background: rgba(17, 24, 39, 0.98);
  backdrop-filter: blur(16px);
  color: #fff;
  padding: 12px 20px;
  border-radius: 12px;
  border: 2px solid rgba(255, 255, 255, 0.15);
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 14px;
  font-weight: 600;
  letter-spacing: 0.3px;
  box-shadow: 0 25px 60px rgba(0, 0, 0, 0.4),
              0 0 0 1px rgba(255, 255, 255, 0.08),
              inset 0 1px 0 rgba(255, 255, 255, 0.1);
  z-index: 2147483647 !important;
  animation: nr-webscraper-info-slide-in 0.3s ease-out;
}

.nr-webscraper-control-overlay__icon {
  width: 18px;
  height: 18px;
  border: 2px solid currentColor;
  border-radius: 50%;
  border-top-color: transparent;
  animation: nr-webscraper-control-spin 1s linear infinite;
}

@keyframes nr-webscraper-control-spin {
  to { transform: rotate(360deg); }
}`;
