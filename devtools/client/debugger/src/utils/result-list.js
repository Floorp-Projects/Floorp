/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function scrollList(resultList, index) {
  if (!resultList.hasOwnProperty(index)) {
    return;
  }

  const resultEl = resultList[index];

  const scroll = () => {
    // Avoid expensive DOM computations involved in scrollIntoView
    // https://nolanlawson.com/2018/09/25/accurately-measuring-layout-on-the-web/
    requestAnimationFrame(() => {
      setTimeout(() => {
        if (!resultEl.scrollIntoView) {
          return;
        }
        resultEl.scrollIntoView({ block: "nearest", behavior: "auto" });
      });
    });
  };

  scroll();
}
