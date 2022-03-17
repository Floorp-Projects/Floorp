/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Assert.throws(() => new EventSource('a'),
              /NS_ERROR_FAILURE/,
              "This should fail, but not crash, in xpcshell");
