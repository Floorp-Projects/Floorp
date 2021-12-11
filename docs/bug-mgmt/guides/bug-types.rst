Bug Types
=========

We organize bugs by type to make it easier to make triage decisions, get
the bug to the right person to make a decision, and understand release
quality.

-  **Defect** regression, crash, hang, security vulnerability and any
   other reported issue
-  **Enhancement** new feature, improvement in UI, performance, etc. and
   any other request for user-facing enhancements to the product, not
   engineering changes
-  **Task** refactoring, removal, replacement, enabling or disabling of
   functionality and any other engineering task

All bug types need triage decisions. Engineering :ref:`triages defects and
tasks <Triage for Bugzilla>`. Product management :ref:`triages
enhancements <New Feature Triage>`.

It’s important to distinguish an enhancement from other types because
they use different triage queues.

Distinguishing between defects and tasks is important because we want to
understand code quality and reduce the number of defects we introduce as
we work on new features and fix existing defects.

When triaging, a task can be as important as a defect. A behind the
scenes change to how a thread is handled can affect performance as seen
by a user.
