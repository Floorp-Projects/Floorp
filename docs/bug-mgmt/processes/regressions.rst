How to Mark Regressions
=======================

Regressions
-----------

For regression bugs in Mozilla-Central, our policy is to tag the bug as
a regression, identify the commits which caused the regression, then
mark the bugs associated with those commits as causing the regression.

What is a regression?
---------------------

A regression is a bug (in our scheme a ``defect``) introduced by a
`changeset <https://en.wikipedia.org/wiki/Changeset>`__.

-  Bug 101 *fixes* Bug 100 with Change Set A
-  Bug 102 *reported which describes previously correct behavior now not
   happening*
-  Bug 102 *investigated and found to be introduced by Change Set A*

Marking a Regression Bug
------------------------

These things are true about regressions:

-  **Bug Type** is ``defect``
-  **Keywords** include ``regression``
-  **Status_FirefoxNN** is ``affected`` for each version (in current
   nightly, beta, and release) of Firefox in which the bug was found
-  The bug’s description covers previously working behavior which is no
   longer working [ed. I need a better phrase for this]

Until the change set which caused the regression has been found through
`mozregression <https://mozilla.github.io/mozregression/>`__ or another
bisection tool, the bug should also have the ``regressionwindow-wanted``
keyword.

Once the change set which caused the regression has been identified,
remove the ``regressionwindow-wanted`` keyword and set the **Regressed
By** field to the id of the bug associated with the change set.

Setting the **Regressed By** field will update the **Regresses** field
in the other bug.

Set a needinfo for the author of the regressing patch asking them to fix
or revert the regression.

Previous Method
---------------

Previously we over-loaded the **Blocks** and **Blocked By** fields to
track the regression, setting **Blocks** to the id of the bug associated
with the change set causing the regression, and using the
``regression``, ``regressionwindow-wanted`` keywords and the status
flags as described above.

This made it difficult to understand what was a dependency and what was
a regression when looking at dependency trees in Bugzilla.

FAQs
----

*To be written*
