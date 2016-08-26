SPRINTF JS UPGRADING

Original library at https://github.com/alexei/sprintf.js
By default the library only supports string placeholders using %s (lowercase) while we use
%S (uppercase). The library has to be manually patched in order to support it.

- grab the unminified version at https://github.com/alexei/sprintf.js/blob/master/src/sprintf.js
- update the re.placeholder regexp to allow "S" as well as "s"
- update the switch statement in the format() method to make case "S" equivalent to case "s"

The original changeset adding support for "%S" can be found on this fork:
- https://github.com/juliandescottes/sprintf.js/commit/a60ea5d7c4cd9a006002ba9f0afc1e2689107eec