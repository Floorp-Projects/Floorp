PlacesTransactions
==================

This module serves as the transactions manager for Places (hereinafter *PTM*).
We need a transaction manager because the bookmarking UI allows users to use
`Undo` and `Redo` functions. To implement transactions a layer has been inserted
between the UI and the Bookmarks API.
This layer stores all the requested changes in a stack and perform calls to the
API. Interall the transactions history is implemented as an array storing
changes from oldest to newest.

Transactions implements all the elementary UI commands: creating items, editing
their properties, and so forth. All the commands are stored in transactions
history and are executed in order.

Constructing transactions
-------------------------

Transactions are exposed by the module as constructors
(e.g. PlacesTransactions.NewFolder). The input for these constructors is taken
in the form of a single argument, a plain object consisting of the properties
for the transaction. Input properties may either be required or optional (for
example, *keyword* is required for the ``EditKeyword`` transaction, but optional
for the ``NewBookmark`` transaction).

Executing Transactions (the `transact` method of transactions)
--------------------------------------------------------------

Once a transaction is created, you must call it's ``transact`` method for it to
be executed and take effect.
That is an asynchronous method that takes no arguments, and returns a promise
that resolves once the transaction is executed.

Executing one of the transactions for creating items (``NewBookmark``,
``NewFolder``, ``NewSeparator``) resolves to the new item's *GUID*.
There's no resolution value for other transactions.

If a transaction fails to execute, ``transact`` rejects and the transactions
history is not affected. As well, ``transact`` throws if it's called more than
once (successfully or not) on the same transaction object.

Batches
-------

Sometimes it is useful to "batch" or "merge" transactions.

For example, something like "Bookmark All Tabs" may be implemented as one
NewFolder transaction followed by numerous NewBookmark transactions - all to be
undone or redone in a single undo or redo command.

Use ``PlacesTransactions.batch`` in such cases, passing an array of transactions
which will be executed in the given order and later be treated a a single entry
in the transactions history. ``PlacesTransactions.batch`` returns a promise
resolved when the batch has been executed. If a transaction depends on results
from a previous one, it can be replaced in the array with a function that takes
``previousArguments`` as only argument, and returns a transaction.

The transactions-history structure
----------------------------------

The transactions-history is a two-dimensional stack of transactions: the
transactions are ordered in reverse to the order they were committed.
It's two-dimensional because PTM allows batching transactions together for the
purpose of undo or redo.

The undoPosition property is set to the index of the top entry. If there is no
entry at that index, there is nothing to undo. Entries prior to undoPosition,
if any, are redo entries, the first one being the top redo entry.

Sources
-------

* :searchfox:`PlacesTransactions.sys.mjs <toolkit/components/places/PlacesTransactions.sys.mjs>`
