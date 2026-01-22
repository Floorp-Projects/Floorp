/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// no solid-js runtime required for this module
import type { GestureDirection } from "../config.ts";
import { getConfig } from "../config.ts";

// Plain DOM implementation using createElementNS; the previous Solid-based
// UI was removed in favor of direct DOM manipulation so we don't depend on
// Solid at runtime for this small visual overlay.

const HTML_NS = "http://www.w3.org/1999/xhtml";

// Track GestureDisplay instances per window to avoid conflicts in multi-window environment
const gestureDisplayInstances = new WeakMap<Window, GestureDisplay>();

export class GestureDisplay {
  private mountContainer: HTMLDivElement | null = null;
  private trail: { x: number; y: number }[] = [];
  private actionName: string = "";
  private visible: boolean = false;
  private feedbackText: string = "";
  private feedbackVisible: boolean = false;
  private directions: GestureDirection[] = [];
  // DOM elements
  private canvasEl: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private labelEl: HTMLDivElement | null = null;
  private resizeHandler: (() => void) | null = null;
  private targetWindow: Window;
  private disposeFn: (() => void) | null = null;

  constructor(win: Window) {
    this.targetWindow = win;

    // Destroy previous instance for this specific window only (not other windows)
    const existingInstance = gestureDisplayInstances.get(win);
    if (existingInstance) {
      existingInstance.destroy();
    }

    gestureDisplayInstances.set(win, this);
    this.addGlobalStyles();
    this.createMountPoint();
    this.initializeComponent();
  }

  private addGlobalStyles(): void {
    if (
      !this.targetWindow.document ||
      !this.targetWindow.document.head
    ) return;

    // Check if styles already exist in this window's document
    if (this.targetWindow.document.getElementById("mouse-gesture-global-styles")) {
      return;
    }

    const styleElement = this.targetWindow.document.createElement("style");
    styleElement.id = "mouse-gesture-global-styles";
    styleElement.textContent = `
            #mouse-gesture-display-container {
                position: fixed;
                top: 0;
                left: 0;
                width: 100vw;
                height: 100vh;
                pointer-events: none;
                z-index: 999999;
                overflow: visible;
            }

            #mouse-gesture-display-container > * {
                pointer-events: none;
            }
        `;
    this.targetWindow.document.head.appendChild(styleElement);
  }

  private createMountPoint(): void {
    if (!this.targetWindow.document || !this.targetWindow.document.body) return;

    const existingContainer = this.targetWindow.document.getElementById(
      "mouse-gesture-display-container",
    );
    if (existingContainer && existingContainer.parentNode) {
      existingContainer.parentNode.removeChild(existingContainer);
    }

    // Ensure styles exist (addGlobalStyles already checks for duplicates)
    this.addGlobalStyles();

    this.mountContainer = this.targetWindow.document.createElement("div");
    this.mountContainer.id = "mouse-gesture-display-container";
    this.targetWindow.document.body.appendChild(this.mountContainer);
  }

  private initializeComponent(): void {
    if (
      !this.mountContainer || !this.targetWindow || !this.targetWindow.document
    ) return;

    // Create canvas and label elements using createElementNS
    const doc = this.targetWindow.document;
    this.canvasEl = doc.createElementNS(HTML_NS, "canvas") as HTMLCanvasElement;
    this.canvasEl.style.position = "fixed";
    this.canvasEl.style.top = "0";
    this.canvasEl.style.left = "0";
    this.canvasEl.style.width = "100vw";
    this.canvasEl.style.height = "100vh";
    this.canvasEl.style.pointerEvents = "none";
    this.canvasEl.style.overflow = "visible";
    this.canvasEl.style.zIndex = "999998";

    this.labelEl = doc.createElementNS(HTML_NS, "div") as HTMLDivElement;
    this.labelEl.style.position = "fixed";
    this.labelEl.style.bottom = "50%";
    this.labelEl.style.left = "50%";
    this.labelEl.style.transform = "translateX(-50%)";
    this.labelEl.style.backgroundColor = "rgba(0, 0, 0, 0.7)";
    this.labelEl.style.color = "white";
    this.labelEl.style.padding = "8px 16px";
    this.labelEl.style.fontWeight = "bold";
    this.labelEl.style.zIndex = "1000000";
    this.labelEl.style.fontSize = "50px";
    this.labelEl.style.display = "none";

    this.mountContainer.appendChild(this.canvasEl);
    this.mountContainer.appendChild(this.labelEl);

    // initial canvas setup
    this.setupCanvas();

    // resize handler
    this.resizeHandler = () => {
      this.setupCanvas();
      this.draw();
    };
    this.targetWindow.addEventListener("resize", this.resizeHandler);

    // dispose function
    this.disposeFn = () => {
      if (this.resizeHandler) {
        this.targetWindow.removeEventListener("resize", this.resizeHandler);
        this.resizeHandler = null;
      }
    };
  }

  private setupCanvas(): void {
    if (!this.canvasEl || !this.targetWindow) return;
    const dpr = this.targetWindow.devicePixelRatio || 1;
    const width = this.targetWindow.innerWidth;
    const height = this.targetWindow.innerHeight;
    this.canvasEl.width = Math.max(1, Math.floor(width * dpr));
    this.canvasEl.height = Math.max(1, Math.floor(height * dpr));
    this.canvasEl.style.width = `${width}px`;
    this.canvasEl.style.height = `${height}px`;
    this.ctx = this.canvasEl.getContext("2d");
    if (this.ctx) {
      this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
  }

  private draw(): void {
    if (!this.canvasEl || !this.ctx) return;
    const dpr = this.targetWindow.devicePixelRatio || 1;
    const w = this.canvasEl.width / dpr;
    const h = this.canvasEl.height / dpr;
    this.ctx.clearRect(0, 0, w, h);

    const trail = this.trail || [];
    if (trail.length < 2) return;

    this.ctx.save();
    this.ctx.lineJoin = "round";
    this.ctx.lineCap = "round";
    this.ctx.globalAlpha = 0.9;
    this.ctx.strokeStyle = getConfig().trailColor || "#37ff00";
    this.ctx.lineWidth = getConfig().trailWidth || 6;
    this.ctx.beginPath();
    this.ctx.moveTo(trail[0].x, trail[0].y);
    for (let i = 1; i < trail.length; i++) {
      this.ctx.lineTo(trail[i].x, trail[i].y);
    }
    this.ctx.stroke();
    this.ctx.restore();
  }

  public show(): void {
    this.visible = true;
    if (this.mountContainer) this.mountContainer.style.display = "block";
  }

  public hide(): void {
    this.visible = false;
    if (this.mountContainer) this.mountContainer.style.display = "none";
  }

  public updateTrail(points: { x: number; y: number }[]): void {
    let newPoints = [...points];

    // Decimate when too many points to reduce draw cost
    const MAX_POINTS = 400;
    if (newPoints.length > MAX_POINTS) {
      const stride = Math.ceil(newPoints.length / MAX_POINTS);
      newPoints = newPoints.filter((_, idx) =>
        idx % stride === 0 || idx === newPoints.length - 1
      );
    }

    //TODO: Performance optimization
    // if (newPoints.length > 100) {
    //     const stride = Math.ceil(newPoints.length);
    //     const optimizedPoints = [];
    //
    //     for (let i = 0; i < newPoints.length; i += stride) {
    //         optimizedPoints.push(newPoints[i]);
    //     }
    //
    //     if (optimizedPoints[optimizedPoints.length - 1] !== newPoints[newPoints.length - 1]) {
    //         optimizedPoints.push(newPoints[newPoints.length - 1]);
    //     }
    //
    //     this.trailSignal[1](optimizedPoints);
    // } else {
    //     this.trailSignal[1](newPoints);
    // }

    this.trail = newPoints;
    this.draw();

    if (!this.visible) {
      this.show();
    }
  }

  public updateActionName(name: string): void {
    this.actionName = name;
    if (this.labelEl) {
      this.labelEl.textContent = name || "";
      this.labelEl.style.display = (name && getConfig().showLabel)
        ? "block"
        : "none";
    }
  }

  public showFeedback(text: string, directions?: GestureDirection[]): void {
    this.feedbackText = text;
    if (directions) this.directions = directions;
    this.feedbackVisible = true;
    if (this.labelEl) {
      this.labelEl.textContent = text;
      this.labelEl.style.display = "block";
    }
  }

  public hideFeedback(): void {
    this.feedbackVisible = false;
    if (this.labelEl) {
      // restore action name if configured, otherwise hide
      if (this.actionName && getConfig().showLabel) {
        this.labelEl.textContent = this.actionName;
        this.labelEl.style.display = "block";
      } else {
        this.labelEl.style.display = "none";
      }
    }
  }

  public destroy(): void {
    if (this.disposeFn) {
      this.disposeFn();
      this.disposeFn = null;
    }

    if (this.mountContainer && this.mountContainer.parentNode) {
      this.mountContainer.parentNode.removeChild(this.mountContainer);
    }

    this.mountContainer = null;
    // Remove from instances map if this is the current instance for the window
    if (gestureDisplayInstances.get(this.targetWindow) === this) {
      gestureDisplayInstances.delete(this.targetWindow);
    }
  }
}
