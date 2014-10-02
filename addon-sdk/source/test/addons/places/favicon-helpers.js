/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Ci, Cu } = require('chrome');
const { Loader } = require('sdk/test/loader');
const loader = Loader(module);
const file = require('sdk/io/file');
const httpd = loader.require('./httpd');
const { pathFor } = require('sdk/system');
const { startServerAsync } = httpd;
const basePath = pathFor('ProfD');
const { atob } = Cu.import("resource://gre/modules/Services.jsm", {});
const historyService = Cc["@mozilla.org/browser/nav-history-service;1"]
                       .getService(Ci.nsINavHistoryService);
const { events } = require('sdk/places/events');

function onFaviconChange (url, callback) {
  function handler ({data, type}) {
    if (type !== 'history-page-changed' ||
        data.url !== url ||
        data.property !== Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON)
      return;
    events.off('data', handler);
    callback(data.value);
  }
  events.on('data', handler);
}
exports.onFaviconChange = onFaviconChange;

/*
 * Takes page content, a page path, and favicon binary data
 */
function serve ({name, favicon, port, host}) {
  let faviconTag = '<link rel="icon" type="image/x-icon" href="/'+ name +'.ico"/>';
  let content = '<html><head>' + faviconTag + '<title>'+name+'</title></head><body></body></html>';
  let srv = startServerAsync(port, basePath);
  let pagePath = file.join(basePath, name + '.html');
  let iconPath = file.join(basePath, name + '.ico');
  let pageStream = file.open(pagePath, 'w');
  let iconStream = file.open(iconPath, 'wb');
  iconStream.write(favicon);
  iconStream.close();
  pageStream.write(content);
  pageStream.close();
  return srv;
}
exports.serve = serve;

let binFavicon = exports.binFavicon = atob('AAABAAEAEBAAAAAAAABoBQAAFgAAACgAAAAQAAAAIAAAAAEACAAAAAAAAAEAAAAAAAAAAAAAAAEAAAABAAAAAAAAAACAAACAAAAAgIAAgAAAAIAAgACAgAAAwMDAAMDcwADwyqYABAQEAAgICAAMDAwAERERABYWFgAcHBwAIiIiACkpKQBVVVUATU1NAEJCQgA5OTkAgHz/AFBQ/wCTANYA/+zMAMbW7wDW5+cAkKmtAAAAMwAAAGYAAACZAAAAzAAAMwAAADMzAAAzZgAAM5kAADPMAAAz/wAAZgAAAGYzAABmZgAAZpkAAGbMAABm/wAAmQAAAJkzAACZZgAAmZkAAJnMAACZ/wAAzAAAAMwzAADMZgAAzJkAAMzMAADM/wAA/2YAAP+ZAAD/zAAzAAAAMwAzADMAZgAzAJkAMwDMADMA/wAzMwAAMzMzADMzZgAzM5kAMzPMADMz/wAzZgAAM2YzADNmZgAzZpkAM2bMADNm/wAzmQAAM5kzADOZZgAzmZkAM5nMADOZ/wAzzAAAM8wzADPMZgAzzJkAM8zMADPM/wAz/zMAM/9mADP/mQAz/8wAM///AGYAAABmADMAZgBmAGYAmQBmAMwAZgD/AGYzAABmMzMAZjNmAGYzmQBmM8wAZjP/AGZmAABmZjMAZmZmAGZmmQBmZswAZpkAAGaZMwBmmWYAZpmZAGaZzABmmf8AZswAAGbMMwBmzJkAZszMAGbM/wBm/wAAZv8zAGb/mQBm/8wAzAD/AP8AzACZmQAAmTOZAJkAmQCZAMwAmQAAAJkzMwCZAGYAmTPMAJkA/wCZZgAAmWYzAJkzZgCZZpkAmWbMAJkz/wCZmTMAmZlmAJmZmQCZmcwAmZn/AJnMAACZzDMAZsxmAJnMmQCZzMwAmcz/AJn/AACZ/zMAmcxmAJn/mQCZ/8wAmf//AMwAAACZADMAzABmAMwAmQDMAMwAmTMAAMwzMwDMM2YAzDOZAMwzzADMM/8AzGYAAMxmMwCZZmYAzGaZAMxmzACZZv8AzJkAAMyZMwDMmWYAzJmZAMyZzADMmf8AzMwAAMzMMwDMzGYAzMyZAMzMzADMzP8AzP8AAMz/MwCZ/2YAzP+ZAMz/zADM//8AzAAzAP8AZgD/AJkAzDMAAP8zMwD/M2YA/zOZAP8zzAD/M/8A/2YAAP9mMwDMZmYA/2aZAP9mzADMZv8A/5kAAP+ZMwD/mWYA/5mZAP+ZzAD/mf8A/8wAAP/MMwD/zGYA/8yZAP/MzAD/zP8A//8zAMz/ZgD//5kA///MAGZm/wBm/2YAZv//AP9mZgD/Zv8A//9mACEApQBfX18Ad3d3AIaGhgCWlpYAy8vLALKysgDX19cA3d3dAOPj4wDq6uoA8fHxAPj4+ADw+/8ApKCgAICAgAAAAP8AAP8AAAD//wD/AAAA/wD/AP//AAD///8ACgoKCgoKCgoKCgoKCgoKCgoKCgoHAQEMbQoKCgoKCgoAAAdDH/kgHRIAAAAAAAAAAADrHfn5ASQQAAAAAAAAAArsBx0B+fkgHesAAAAAAAD/Cgwf+fn5IA4dEus/IvcACgcMAfkg+QEB+SABHushbf8QHR/5HQH5+QEdHetEHx4K7B/5+QH5+fkdDBL5+SBE/wwdJfkf+fn5AR8g+fkfEArsCh/5+QEeJR/5+SAeBwAACgoe+SAlHwFAEhAfAAAAAPcKHh8eASYBHhAMAAAAAAAA9EMdIB8gHh0dBwAAAAAAAAAA7BAdQ+wHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP//AADwfwAAwH8AAMB/AAAAPwAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAQAAgAcAAIAPAADADwAA8D8AAP//AAA');
