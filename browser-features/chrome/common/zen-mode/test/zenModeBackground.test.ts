// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import zenCSS from "../zen-mode.css?inline";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function testOverlayBackground(): void {
  const root = document.documentElement;
  const toolbox = document.getElementById("navigator-toolbox")!;
  const savedStyle = toolbox.getAttribute("style");
  const savedZen = root.getAttribute("zenmode");
  const savedReveal = root.getAttribute("zenmode-reveal-top");
  const style = document.createElement("style");
  style.textContent = zenCSS;

  try {
    root.removeAttribute("zenmode");
    const normalColor = getComputedStyle(toolbox)!.backgroundColor;
    const normalImage = getComputedStyle(toolbox)!.backgroundImage;
    document.head!.append(style);
    root.setAttribute("zenmode", "true");
    root.setAttribute("zenmode-reveal-top", "true");

    for (const color of ["rgb(240, 240, 244)", "rgb(28, 27, 34)"]) {
      toolbox.style.setProperty("--toolbox-background-color", color);
      // An inactive window must use the inactive palette too.
      toolbox.style.setProperty("--toolbox-background-color-inactive", color);
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
