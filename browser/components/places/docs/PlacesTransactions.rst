PlacesTransactions.jsm
======================

This module serves as the transactions manager for Places (hereinafter *PTM*). Generally, we need a transaction manager because History and Bookmark UI allows users to use `Undo / Redo` functions. To implement those transaction History and Bookmark needed to have a layer between Bookmark API and History API. This layer stores all requested changes in a stack and perform calls to API. That construction allows users perform `Undo / Redo` simply removing / adding that transaction and call to API from an array. Inside array we store changes from oldest to newest.

Transactions implements all the elementary UI commands: creating items, editing their various properties, and so forth. All those commands stored in array and are completed one after another.


Constructing transactions
-------------------------

Transactions are exposed by the module as constructors (e.g. PlacesTransactions.NewFolder). The input for these constructors is taken in the form of a single argument, a plain object consisting of the properties for the transaction. Input properties may be either required or optional (for example, *keyword* is required for the ``EditKeyword`` transaction, but optional for the ``NewBookmark`` transaction).

Executing Transactions (the `transact` method of transactions)
--------------------------------------------------------------

Once a transaction is created, you must call it's *transact* method for it to be executed and take effect. *Transact* is an asynchronous method that takes no arguments, and returns a promise that resolves once the transaction is executed.

Executing one of the transactions for creating items (``NewBookmark``, ``NewFolder``, ``NewSeparator``) resolves to the new item's *GUID*.

There's no resolution value for other transactions. If a transaction fails to execute, *transact* rejects and the transactions history is not affected. As well, *transact* throws if it's called more than once (successfully or not) on the same transaction object.

Batches
-------

Sometimes it is useful to "batch" or "merge" transactions.

For example, something like "Bookmark All Tabs" may be implemented as one NewFolder transaction followed by numerous NewBookmark transactions - all to be undone or redone in a single undo or redo command.

Using ``PlacesTransactions.batch`` in such cases can take either an array of transactions which will be executed in the given order and later be treated a a single entry in the transactions history. Once the generator function is called a batch starts, and it lasts until the asynchronous generator iteration is complete.

``PlacesTransactions.batch`` returns a promise that is to be resolved when the batch ends. “Nested" batches are not supported, if you call batch while another batch is still running, the new batch is enqueued with all other PTM work and thus not run until the running batch ends. The same goes for undo, redo and clearTransactionsHistory.

It’s important not to await any promise from batch function for such methods, as: undo, redo, clearTransactionsHistory. Doing so causing a complete shutdown of PlacesTransactionManager, not allowing execution of any transactions.

The transactions-history structure
----------------------------------

The transactions-history is a two-dimensional stack of transactions: the transactions are ordered in reverse to the order they were committed. It's two-dimensional because PTM allows batching transactions together for the purpose of undo or redo.

The undoPosition property is set to the index of the top entry. If there is no entry at that index, there is nothing to undo. Entries prior to undoPosition, if any, are redo entries, the first one being the top redo entry.

Full file with actual javadoc and description of each method - `PlacesTransactions.jsm`_
