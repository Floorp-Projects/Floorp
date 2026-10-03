// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import zenCSS from "../zen-mode.css?inline";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function waitFor(check: () => boolean, message: string): Promise<void> {
  for (let i = 0; i < 250; i++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  assert(check(), message);
}

async function settleBackground(toolbox: Element): Promise<void> {
  // Removing [zenmode] restores Lepton's background-color transition. A
  // synchronous read sees the outgoing ActiveCaption, not the normal color.
  getComputedStyle(toolbox)!.backgroundColor;
  await waitFor(
    () =>
      toolbox.getAnimations().every((animation) =>
        animation.playState !== "running"
      ),
    "Toolbox transitions must finish before checking its normal background",
  );
}

async function testOverlayBackground(): Promise<void> {
  const root = document.documentElement;
  const toolbox = document.getElementById("navigator-toolbox")!;
  const savedStyle = toolbox.getAttribute("style");
  const savedZen = root.getAttribute("zenmode");
  const savedReveal = root.getAttribute("zenmode-reveal-top");
  const style = document.createElement("style");
  style.textContent = zenCSS;
  let focusWindow: Window | null = null;

  try {
    root.removeAttribute("zenmode");
    window.focus();
    await waitFor(
      () => !root.matches(":-moz-window-inactive"),
      "The browser must be active before checking its active palette",
    );
    await settleBackground(toolbox);
    const normalColor = getComputedStyle(toolbox)!.backgroundColor;
    const normalImage = getComputedStyle(toolbox)!.backgroundImage;
    document.head!.append(style);
    root.setAttribute("zenmode", "true");
    root.setAttribute("zenmode-reveal-top", "true");

    for (const color of ["rgb(240, 240, 244)", "rgb(28, 27, 34)"]) {
      toolbox.style.setProperty("--toolbox-background-color", color);
      toolbox.style.setProperty(
        "--toolbox-background-color-inactive",
        "rgb(71, 83, 97)",
      );
      toolbox.style.setProperty("--toolbox-background-image", "none");
      const computed = getComputedStyle(toolbox)!;
      assert(
        computed.backgroundImage.includes(color),
        "The overlay must paint the current light/dark frame color",
      );
      assert(
        computed.backgroundColor !== "rgba(0, 0, 0, 0)",
        "The overlay must provide an opaque system backing",
      );
    }

    // This is a document/window state, not an element pseudo-class that can
    // be forced with InspectorUtils.addPseudoClassLock. Move actual focus to
    // a disposable chrome window and require the native selector to match.
    focusWindow = window.openDialog(
      "about:blank",
      "_blank",
      "chrome,dialog=no,width=200,height=100",
    );
    assert(focusWindow, "A disposable focus window must open");
    await waitFor(
      () => focusWindow?.document?.readyState === "complete",
      "The focus window must finish loading",
    );
    focusWindow.focus();
    await waitFor(
      () => root.matches(":-moz-window-inactive"),
      "The browser must actually match the inactive-window selector",
    );
    const inactive = getComputedStyle(toolbox)!;
    assert(
      inactive.backgroundImage.includes("rgb(71, 83, 97)") &&
        !inactive.backgroundImage.includes("rgb(28, 27, 34)"),
      "The inactive override must paint the distinct inactive frame color",
    );
    focusWindow.close();
    focusWindow = null;
    window.focus();
    await waitFor(
      () => !root.matches(":-moz-window-inactive"),
      "Closing the focus window must restore the browser's active state",
    );

    // Extension themes can supply several images and a translucent frame.
    // Preserve all layers and their individual positioning above the backing.
    const artwork = "linear-gradient(rgb(31, 91, 151), rgb(211, 71, 31))";
    toolbox.style.setProperty(
      "--toolbox-background-image",
      `${artwork}, ${artwork}`,
    );
    toolbox.style.setProperty(
      "--toolbox-background-repeat",
      "repeat-x, no-repeat",
    );
    toolbox.style.setProperty(
      "--toolbox-background-position",
      "left top, right bottom",
    );
    toolbox.style.setProperty(
      "--toolbox-background-size",
      "40px 30px, 80px 60px",
    );
    toolbox.style.setProperty(
      "--toolbox-background-color",
      "rgba(20, 30, 40, 0.5)",
    );
    toolbox.style.setProperty(
      "--toolbox-background-color-inactive",
      "rgba(20, 30, 40, 0.5)",
    );
    const themed = getComputedStyle(toolbox)!;
    assert(
      themed.backgroundImage.startsWith(`${artwork}, ${artwork}, `),
      "Both theme images must remain above the frame color",
    );
    assert(
      themed.backgroundImage.includes("rgba(20, 30, 40, 0.5)"),
      "Translucent theme colors must remain above the opaque backing",
    );
    assertEquals(
      themed.backgroundRepeat,
      "repeat-x, no-repeat, no-repeat",
      "Theme tiling survives",
    );
    assertEquals(
      themed.backgroundPosition,
      "0% 0%, 100% 100%, 100% 0%",
      "Theme positioning survives",
    );
    assertEquals(
      themed.backgroundSize,
      "40px 30px, 80px 60px, auto",
      "Theme sizing survives",
    );

    root.removeAttribute("zenmode");
    if (savedStyle === null) toolbox.removeAttribute("style");
    else toolbox.setAttribute("style", savedStyle);
    await settleBackground(toolbox);
    assertEquals(
      getComputedStyle(toolbox)!.backgroundColor,
      normalColor,
      "Leaving Zen restores the normal background",
    );
    assertEquals(
      getComputedStyle(toolbox)!.backgroundImage,
      normalImage,
      "Leaving Zen restores the normal artwork placement",
    );
  } finally {
    focusWindow?.close();
    style.remove();
    if (savedStyle === null) toolbox.removeAttribute("style");
    else toolbox.setAttribute("style", savedStyle);
    if (savedZen === null) root.removeAttribute("zenmode");
    else root.setAttribute("zenmode", savedZen);
    if (savedReveal === null) root.removeAttribute("zenmode-reveal-top");
    else root.setAttribute("zenmode-reveal-top", savedReveal);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("zenModeBackground.test.ts", [{
    name:
      "Zen overlay owns an opaque themed background and restores normal mode",
    fn: testOverlayBackground,
  }]);
}
