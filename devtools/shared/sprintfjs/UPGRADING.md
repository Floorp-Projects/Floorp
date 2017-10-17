SPRINTF JS UPGRADING

Original library at https://github.com/alexei/sprintf.js

This library should no longer be upgraded from upstream. We added performance improvements
in https://bugzilla.mozilla.org/show_bug.cgi?id=1406311. Most importantly removing the
usage of the get_type() method as well as prioritizing the %S use case.

If for some reason, updating from upstream becomes necessary, please refer to the bug
mentioned above to reimplement the performance fixes in the new version.

By default the library only supports string placeholders using %s (lowercase) while we use
%S (uppercase). The library also has to be manually patched in order to support it.

- grab the unminified version at https://github.com/alexei/sprintf.js/blob/master/src/sprintf.js
- update the re.placeholder regexp to allow "S" as well as "s"
- update the switch statement in the format() method to make case "S" equivalent to case "s"
