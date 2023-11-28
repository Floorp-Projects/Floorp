What Needs My Attention
=======================

Bugzilla’s `What needs my attention?`_ (a.k.a What Should I Work On Next) dashboard helps us to focus on the top-most important or urgent engineering tasks for releasing Firefox.

The dashboard is not designed for including everything on a person’s plate. It doesn’t attempt to prioritize normal work - it is just for the things that are more important than the normal stuff. It’s about individual engineer prioritization rather than team prioritization and it doesn’t claim to include all sources of high priority work (e.g. triage, responding to requests from HR, etc).

The dashboard is available to Mozilla engineers, using the icon

	.. image:: ../assets/icon_assignments.png
	  :alt: Notification Icon

in the top right hand corner, after you log into Bugzilla.

The dashboard is a collection of some (sometimes non-obvious) Bugzilla searches. **Web Platform engineers** should check this page once per day, and ideally keep the list empty, so we can focus on our normal or planned work.


.. _What needs my attention?: https://bugzilla.mozilla.org/page.cgi?id=whats_next.html


Rules of thumb
--------------

Here are some rules of thumb about our priorities when it comes to bug fixing.


Highest priority tasks
~~~~~~~~~~~~~~~~~~~~~~

These are the things you should drop everything else for. Generally, work where you block others should be addressed as higher priority than non-blocking work.

#. Critical needinfos

	* Bugs that are needinfo? you and are marked as Severity = S1 defects or with the “sec-critical” keyword.

	* Bugs that are needinfo? you and are marked as being tracked against or blocking the current beta release. These bugs are potentially S2 once they are triaged and important. If we don’t act on these bugs, we’re in danger of delaying a release.

	* Bugs that are needinfo? you and are marked as a security issue without rating. These bugs are potentially sec-critical or sec-high once they are triaged and important.

#. Review requests for bugs that are marked as Severity = S1 defects or with the “sec-critical” keyword.

	* Code review requests are not visible on this dashboard, please visit Phabricator to view those. In general it’s reasonable and important to prioritize Review Requests to unblock others.

#. Critical bugs assigned to you

	* Severity = S1 defects and bugs with the “sec-critical” keyword.


High priority tasks
~~~~~~~~~~~~~~~~~~~

High priority tasks are also “drop everything”, except that in this case “everything” doesn’t include anything in the “Highest priority” list. Generally, work where you block others should be addressed as higher priority than non-blocking work.

#. Important needinfos

	* Bugs that are needinfo? you and are marked as Severity = S2 defects or with the “sec-high” keyword.

#. Review requests for bugs that are marked as Severity = S2 defects or with the “sec-high” keyword.

	* Code review requests are not visible on this dashboard, please visit Phabricator to view those. In general it’s reasonable and important to prioritize Review Requests to unblock others.

#. Important bugs assigned to you

	* Severity = S2 defects and bugs with the “sec-high” keyword (both for things that are not disabled in the current release)
	* Note: Some teams have very long lists of S2 defects - see notes below on “Long High Priority task lists”

#. Your other needinfos (except for things that are self-needinfo).


Handling needinfos
------------------

TL;DR: Don’t leave people hanging. Reply and convert needinfos from others to self-needinfos.

Many people have long lists of needinfos. Please don’t ignore them. Here’s how we suggest you burn them down, and keep them down:

* Any needinfo older than 3 months has probably been forgotten about by the requester. It’s okay to declare needinfo bankruptcy. If you’re concerned about annoying the requester by clearing the needinfo, feel free to point them at this document and offer that they can re-request if there is still a problem.

* For newer requests, don’t leave someone or something, .e.g BugBot hanging. If you can take action in the short term, do so. If you can’t do it straight away, reply letting the requester know that you intend to work on it, but can’t do so immediately. This clears the original needinfo. At the same time you can re-request a needinfo (using the “Request information from ‘myself’” feature). This converts the needinfo to a self-needinfo, and takes it off the attention list. Be mindful, do the conversion when you have given the right attention and response so that others are not hanging on there, or you've done the best you can for the time being to unblock others. We don't want it to become a way to ignore requests.


Review requests
---------------

This list doesn’t include Review Requests as as we are still investigating the feasibility of including them and applying these strict rules, but we might consider adding this to a future revision.

In the meanwhile, it’s worthwhile considering the use of peer review groups set up in Phabricator so that multiple engineers can assist in reviews.


Other notes
-----------

* Long lists of High Priority tasks: For some people and teams, the list of “High Priority” tasks is so long that you would never do normal work. If this is you then you should schedule these tasks alongside normal work. However making your task list manageable should still be a priority.

* `Severity`_ is defined, but things get a bit hazy when it comes to how we define severity for enhancements; this list is for serious defects only.

.. _Severity: https://firefox-source-docs.mozilla.org/bug-mgmt/guides/severity.html


Everything else
---------------

This list is not designed for including everything or prioritizing your normal work. Over time we’d like to bring teams’ practices for prioritizing new work more in line with each other, but that’s not the job of this note.

If you find that most of your time is spent on high or highest priority tasks, then it’s time to ask some questions to work out why - there’s likely to be a problem behind this and it sounds like a recipe for burnout, and we should do everything we can to even things out.
