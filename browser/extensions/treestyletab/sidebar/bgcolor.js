/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

{
  const url = new URL(location.href);

  const bgcolor = url.searchParams.get('bgcolor');
  if (bgcolor)
    document.querySelector('#bgcolor').textContent = `
      :root,
      body,
      #background {
        background-color: ${bgcolor};
      }
    `.trim();
}
