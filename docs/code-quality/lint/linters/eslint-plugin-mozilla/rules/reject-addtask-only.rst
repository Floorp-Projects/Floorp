reject-addtask-only
===================

Designed for JavaScript tests using the add_task pattern. Rejects chaining
.only() to an add_task() call, which is useful for local testing to run a
single task in isolation but is easy to land into the tree by accident.
