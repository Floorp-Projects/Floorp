/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRoot, createSignal, Show } from "solid-js";
import { render } from "@nora/solid-xul";
import type { GestureDirection } from "../config.ts";
import { getConfig } from "../config.ts";

export function GestureDisplayUI(props: {
  trail: { x: number; y: number }[];
  actionName: string;
  isVisible: boolean;
  feedbackVisible?: boolean;
  directions?: GestureDirection[];
}) {
  const getTrailElements = () => {
    if (props.trail.length < 2 || !getConfig().showTrail) {
      return null;
    }

    const config = getConfig();
    const trailColor = config.trailColor || "#37ff00";
    const trailWidth = config.trailWidth || 6;

    return props.trail.map((point, index) => {
      if (index === 0) return null;

      const prevPoint = props.trail[index - 1];
      const dx = point.x - prevPoint.x;
      const dy = point.y - prevPoint.y;
      const length = Math.sqrt(dx * dx + dy * dy);
      const angle = Math.atan2(dy, dx) * 180 / Math.PI;

      if (length < 0.1) return null;

      const displayLength = Math.max(length, 2);
      const displayWidth = Math.max(trailWidth, 3);
      const opacity = length < 5 ? 0.9 : 0.7;

      return (
        <div
          style={{
            position: "fixed",
            height: `${displayWidth}px`,
            width: `${displayLength}px`,
            "background-color": trailColor,
            opacity: opacity.toString(),
            left: `${prevPoint.x}px`,
            top: `${prevPoint.y}px`,
            transform: `rotate(${angle}deg)`,
            "transform-origin": "left center",
            "pointer-events": "none",
            "border-radius": "1px",
            "box-shadow": `0 0 2px ${trailColor}`,
          }}
        />
      );
    });
  };

  return (
    <>
      <Show when={props.isVisible}>
        <div
          style={{
            position: "fixed",
            top: "0",
            left: "0",
            width: "100vw",
            height: "100vh",
            "pointer-events": "none",
            "z-index": "999999",
          }}
        >
          <div
            style={{
              position: "fixed",
              top: "0",
              left: "0",
              width: "100%",
              height: "100%",
              "pointer-events": "none",
              overflow: "visible",
            }}
          >
            {getTrailElements()}
          </div>

          <Show when={props.actionName && getConfig().showLabel}>
            <div
              style={{
                position: "fixed",
                bottom: "50%",
                left: "50%",
                transform: "translateX(-50%)",
                "background-color": "rgba(0, 0, 0, 0.7)",
                color: "white",
                padding: "8px 16px",
                "font-weight": "bold",
                "z-index": "1000000",
                "font-size": "50px",
              }}
            >
              {props.actionName}
            </div>
          </Show>
        </div>
      </Show>
    </>
  );
}

let gestureDisplayInstance: GestureDisplay | null = null;
let styleAdded = false;

export class GestureDisplay {
  private mountContainer: HTMLDivElement | null = null;
  private trailSignal = createSignal<{ x: number; y: number }[]>([]);
  private actionSignal = createSignal<string>("");
  private visibleSignal = createSignal<boolean>(false);
  private feedbackTextSignal = createSignal<string>("");
  private feedbackVisibleSignal = createSignal<boolean>(false);
  private directionsSignal = createSignal<GestureDirection[]>([]);
  private disposeFn: (() => void) | null = null;

  constructor() {
    if (gestureDisplayInstance) {
      gestureDisplayInstance.destroy();
    }

    // eslint-disable-next-line no-this-alias
    gestureDisplayInstance = this;
    this.addGlobalStyles();
    this.createMountPoint();
    this.initializeComponent();
  }

  private addGlobalStyles(): void {
    if (styleAdded || !document || !document.head) return;

    const styleElement = document.createElement("style");
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

            #mouse-gesture-display-container * {
                position: fixed;
                pointer-events: none;
            }
        `;
    document.head.appendChild(styleElement);
    styleAdded = true;
  }

  private createMountPoint(): void {
    if (!document || !document.body) return;

    const existingContainer = document.getElementById(
      "mouse-gesture-display-container",
    );
    if (existingContainer && existingContainer.parentNode) {
      existingContainer.parentNode.removeChild(existingContainer);
    }

    const existingStyle = document.getElementById(
      "mouse-gesture-global-styles",
    );
    if (existingStyle && existingStyle.parentNode) {
      existingStyle.parentNode.removeChild(existingStyle);
      styleAdded = false;
      this.addGlobalStyles();
    }

    this.mountContainer = document.createElement("div");
    this.mountContainer.id = "mouse-gesture-display-container";
    document.body.appendChild(this.mountContainer);
  }

  private initializeComponent(): void {
    if (!this.mountContainer) return;

    const dispose = createRoot((dispose) => {
      const [trail] = this.trailSignal;
      const [actionName] = this.actionSignal;
      const [isVisible] = this.visibleSignal;
      const [feedbackVisible] = this.feedbackVisibleSignal;
      const [directions] = this.directionsSignal;

      render(
        () => (
          <GestureDisplayUI
            trail={trail()}
            actionName={actionName()}
            isVisible={isVisible()}
            feedbackVisible={feedbackVisible()}
            directions={directions()}
          />
        ),
        this.mountContainer,
        {
          hotCtx: import.meta.hot,
        },
      );

      return dispose;
    });

    this.disposeFn = dispose;
  }

  public show(): void {
    this.visibleSignal[1](true);
  }

  public hide(): void {
    this.visibleSignal[1](false);
  }

  public updateTrail(points: { x: number; y: number }[]): void {
    const newPoints = [...points];

    //TODO: Performance optimization
    // if (newPoints.length > 100) {
    //     const stride = Math.ceil(newPoints.length);
    //     const optimizedPoints = [];

    //     for (let i = 0; i < newPoints.length; i += stride) {
    //         optimizedPoints.push(newPoints[i]);
    //     }

    //     if (optimizedPoints[optimizedPoints.length - 1] !== newPoints[newPoints.length - 1]) {
    //         optimizedPoints.push(newPoints[newPoints.length - 1]);
    //     }

    //     this.trailSignal[1](optimizedPoints);
    // } else {
    //     this.trailSignal[1](newPoints);
    // }

    this.trailSignal[1](newPoints);

    if (!this.visibleSignal[0]()) {
      this.visibleSignal[1](true);
    }
  }

  public updateActionName(name: string): void {
    this.actionSignal[1](name);
  }

  public showFeedback(text: string, directions?: GestureDirection[]): void {
    this.feedbackTextSignal[1](text);
    if (directions) {
      this.directionsSignal[1](directions);
    }
    this.feedbackVisibleSignal[1](true);
  }

  public hideFeedback(): void {
    this.feedbackVisibleSignal[1](false);
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

    if (gestureDisplayInstance === this) {
      gestureDisplayInstance = null;
    }
  }
}
