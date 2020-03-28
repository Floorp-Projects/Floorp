New Feature Triage
==================

Identifying Feature Requests
----------------------------

Bugs which request new features or enhancements should be of
type=\ ``enhancement``.

Older bugs may also be feature requests if some or all of the following
are true:

-  ~Bugs marked severity = ``enhancement``\ ~ *(This has gone away with
   the switch to*\ `bug
   types <https://bugzilla.mozilla.org/show_bug.cgi?id=1522340>`__\ *)*
-  Bugs with ``feature`` or similar in whiteboard or short description
-  ``[RFE]`` in whiteboard, short description, or description
-  Bugs not explicitly marked as a feature request, but appear to be
   feature requests
-  Bugs marked with ``feature`` keyword

Initial Triage
--------------

Staff, contractors, and other contributors looking at new bugs in
*Firefox::Untriaged* and *::General* components should consider if a
bug, if not marked as a feature request, should be one, and if so:

-  Update the bug’s type to ``enhancement``
-  Determine which product and component the bug belongs to and update
   it **or**

   -  Use *needinfo* to ask a component’s triage owner or a module’s
      owner where the request should go

Product Manager Triage
----------------------

-  The product manager for the component reviews bugs of type
   ``enhancement``

   -  This review should be done a least weekly

-  Reassigns to another Product::Component if necessary **or**
-  Determines next steps

   -  Close bug as ``RESOLVED WONTFIX`` with comment as to why and
      thanking submitter
   -  If bug is similar enough to work in roadmap, close bug as
      ``RESOLVED DUPLICATE`` of the feature bug it is similar to

      -  If there’s not a feature bug created already, then consider
         making this bug the feature bug

         -  Set type to ``enhancement``

   -  Set bug to ``P5`` priority with comment thanking submitter, and
      explaining that the request will be considered for future roadmaps
