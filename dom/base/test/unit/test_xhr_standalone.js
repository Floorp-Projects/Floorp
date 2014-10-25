/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test setting .responseType and .withCredentials is allowed
// in non-window non-Worker context

function run_test()
{
    var xhr = Components.classes['@mozilla.org/xmlextras/xmlhttprequest;1'].
          createInstance(Components.interfaces.nsIXMLHttpRequest);
    xhr.open('GET', 'data:,', false);
    var exceptionThrown = false;
    try {
        xhr.responseType = '';
        xhr.withCredentials = false;
    } catch (e) {
        exceptionThrown = true;
    }
    do_check_eq(false, exceptionThrown);
}
