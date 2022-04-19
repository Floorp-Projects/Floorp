Release Status Flags
====================

The flag ``status_firefoxNN`` has many values, here’s a cheat sheet.

== ========== ========== ============ =================
—  ?          unaffected affected     fixed
== ========== ========== ============ =================
?  unaffected            wontfix      verified
\  affected              fix-optional disabled
\                        fixed        verified disabled
== ========== ========== ============ =================

The headers of the table are values of the status flag. Each column are
the states reachable from the column headings.

-  ``---`` we don’t know whether Firefox N is affected
-  ``?`` we don’t know whether Firefox N is affected, but we want to find
   out.
-  ``affected`` - present in this release
-  ``unaffected`` - not present in this release
-  ``fixed`` - a contributor has landed a change set in the tree
   to address the issue
-  ``verified`` - the fix has been verified by QA or other contributors
-  ``disabled`` - the fix or the feature has been backed out or disabled
-  ``verified disabled`` - QA or other contributors confirmed the fix or
   the feature has been backed out or disabled
-  ``wontfix`` - we have decided not to accept/uplift a fix for this
   release cycle (it is not the same as the bug resolution WONTFIX).
   This can also mean that we don’t know how to fix that and will ship
   with this bug
-  ``fix-optional`` - we would take a fix for the current release but
   don’t consider it as important/blocking for the release
