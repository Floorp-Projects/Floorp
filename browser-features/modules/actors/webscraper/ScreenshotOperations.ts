/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ScreenshotOperations - Screenshot capture functionality
 */

import type { ScreenshotRect, WebScraperContext } from "./types.ts";

/**
 * Helper class for taking screenshots
 */
export class ScreenshotOperations {
  constructor(private context: WebScraperContext) {}

  get contentWindow(): (Window & typeof globalThis) | null {
    return this.context.contentWindow;
  }

  get document(): Document | null {
    return this.context.document;
  }

  /**
   * Takes a screenshot of the current viewport
   */
  async takeScreenshot(): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const canvas = (await this.contentWindow.document?.createElement(
        "canvas",
      )) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;
      const { innerWidth, innerHeight } = this.contentWindow;

      if (!innerWidth || !innerHeight) {
        console.warn(
          "ScreenshotOperations: viewport size is zero; aborting screenshot",
        );
        return null;
      }

      canvas.width = innerWidth;
      canvas.height = innerHeight;

      ctx.drawWindow(
        this.contentWindow,
        0,
        0,
        innerWidth,
        innerHeight,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error("ScreenshotOperations: Error taking screenshot:", e);
      return null;
    }
  }

  /**
   * Takes a screenshot of a specific element
   */
  async takeElementScreenshot(selector: string): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const element = this.document?.querySelector(selector) as HTMLElement;

      if (!element) {
        return null;
      }

      const canvas = (await this.contentWindow.document?.createElement(
        "canvas",
      )) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;
      const rect = element.getBoundingClientRect();

      if (!rect.width || !rect.height) {
        console.warn(
          "ScreenshotOperations: target element has zero size; aborting element screenshot",
        );
        return null;
      }

      canvas.width = rect.width;
      canvas.height = rect.height;

      const scrollX = this.contentWindow.scrollX || 0;
      const scrollY = this.contentWindow.scrollY || 0;

      ctx.drawWindow(
        this.contentWindow,
        rect.left + scrollX,
        rect.top + scrollY,
        rect.width,
        rect.height,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error(
        "ScreenshotOperations: Error taking element screenshot:",
        e,
      );
      return null;
    }
  }

  /**
   * Takes a screenshot of the full page
   */
  async takeFullPageScreenshot(): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const doc = this.contentWindow.document;
      const canvas = (await doc?.createElement("canvas")) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

      const width = doc?.documentElement?.scrollWidth ?? 0;
      const height = doc?.documentElement?.scrollHeight ?? 0;

      if (!width || !height) {
        console.warn(
          "ScreenshotOperations: page dimensions are zero; aborting full page screenshot",
        );
        return null;
      }

      canvas.width = width;
      canvas.height = height;

      ctx.drawWindow(
        this.contentWindow,
        0,
        0,
        width,
        height,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error(
        "ScreenshotOperations: Error taking full page screenshot:",
        e,
      );
      return null;
    }
  }

  /**
   * Takes a screenshot of a specific region
   */
  async takeRegionScreenshot(rect: ScreenshotRect): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const doc = this.contentWindow.document;
      const canvas = (await doc?.createElement("canvas")) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

      const pageScrollWidth = doc?.documentElement?.scrollWidth ?? 0;
      const pageScrollHeight = doc?.documentElement?.scrollHeight ?? 0;

      const x = rect.x ?? 0;
      const y = rect.y ?? 0;
      const width = rect.width ?? pageScrollWidth - x;
      const height = rect.height ?? pageScrollHeight - y;

      const captureX = Math.max(0, x);
      const captureY = Math.max(0, y);
      const captureWidth = Math.min(width, pageScrollWidth - captureX);
      const captureHeight = Math.min(height, pageScrollHeight - captureY);

      if (captureWidth <= 0 || captureHeight <= 0) {
        console.warn(
          "ScreenshotOperations: requested region is empty; aborting region screenshot",
        );
        return null;
      }

      canvas.width = captureWidth;
      canvas.height = captureHeight;

      ctx.drawWindow(
        this.contentWindow,
        captureX,
        captureY,
        captureWidth,
        captureHeight,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error("ScreenshotOperations: Error taking region screenshot:", e);
      return null;
    }
  }
}
