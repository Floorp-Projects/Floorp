/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Frame } from "../../../types";
import { getFrameUrl } from "./getFrameUrl";

const libraryMap = [
  {
    label: "Backbone",
    pattern: /backbone/i,
  },
  {
    label: "Babel",
    pattern: /node_modules\/@babel/i,
  },
  {
    label: "jQuery",
    pattern: /jquery/i,
  },
  {
    label: "Preact",
    pattern: /preact/i,
  },
  {
    label: "React",
    pattern: /(node_modules\/(?:react|react-dom)\/)|(react(-dom)?(\.[a-z]+)*\.js$)/,
  },
  {
    label: "Immutable",
    pattern: /immutable/i,
  },
  {
    label: "Webpack",
    pattern: /webpack\/bootstrap/i,
  },
  {
    label: "Express",
    pattern: /node_modules\/express/,
  },
  {
    label: "Pug",
    pattern: /node_modules\/pug/,
  },
  {
    label: "ExtJS",
    pattern: /\/ext-all[\.\-]/,
  },
  {
    label: "MobX",
    pattern: /mobx/i,
  },
  {
    label: "Underscore",
    pattern: /underscore/i,
  },
  {
    label: "Lodash",
    pattern: /lodash/i,
  },
  {
    label: "Ember",
    pattern: /ember/i,
  },
  {
    label: "Choo",
    pattern: /choo/i,
  },
  {
    label: "VueJS",
    pattern: /vue(?:\.[a-z]+)*\.js/i,
  },
  {
    label: "RxJS",
    pattern: /rxjs/i,
  },
  {
    label: "Angular",
    pattern: /angular(?!.*\/app\/)/i,
    contextPattern: /zone\.js/,
  },
  {
    label: "Redux",
    pattern: /redux/i,
  },
  {
    label: "Dojo",
    pattern: /dojo/i,
  },
  {
    label: "Marko",
    pattern: /marko/i,
  },
  {
    label: "NuxtJS",
    pattern: /[\._]nuxt/i,
  },
  {
    label: "Aframe",
    pattern: /aframe/i,
  },
  {
    label: "NextJS",
    pattern: /[\._]next/i,
  },
];

export function getLibraryFromUrl(
  frame: Frame,
  callStack: Array<Frame> = []
): ?string | void {
  // @TODO each of these fns calls getFrameUrl, just call it once
  // (assuming there's not more complex logic to identify a lib)
  const frameUrl = getFrameUrl(frame);

  // Let's first check if the frame match a defined pattern.
  let match = libraryMap.find(o => o.pattern.test(frameUrl));
  if (match) {
    return match.label;
  }

  // If it does not, it might still be one of the case where the file is used
  // by a library but the name has not enough specificity. In such case, we want
  // to only return the library name if there are frames matching the library
  // pattern in the callStack (e.g. `zone.js` is used by Angular, but the name
  //  could be quite common and return false positive if evaluated alone. So we
  // only return Angular if there are other frames matching Angular).
  match = libraryMap.find(
    o => o.contextPattern && o.contextPattern.test(frameUrl)
  );
  if (match) {
    const contextMatch = callStack.some(f => {
      const url = getFrameUrl(f);
      if (!url) {
        return false;
      }

      return libraryMap.some(o => o.pattern.test(url));
    });

    if (contextMatch) {
      return match.label;
    }
  }

  return null;
}
