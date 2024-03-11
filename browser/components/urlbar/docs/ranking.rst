=======
Ranking
=======

Before results appear in the UrlbarView, they are fetched from providers.

Each `UrlbarProvider <https://firefox-source-docs.mozilla.org/browser/urlbar/overview.html#urlbarprovider>`_
implements its own internal ranking and returns sorted results.

Externally all the results are ranked by the `UrlbarMuxer <https://searchfox.org/mozilla-central/source/browser/components/urlbar/UrlbarMuxerUnifiedComplete.sys.mjs>`_
according to an hardcoded list of groups and sub-grups.

.. NOTE:: Preferences can influence the groups order, for example by putting
  Firefox Suggest before Search Suggestions.

The Places provider, responsible to return history and bookmark results, uses
an internal ranking algorithm called Frecency.

Frecency implementation
=======================

Frecency is a term derived from `frequency` and `recency`, its scope is to provide a
ranking algorithm that gives importance both to how often a page is accessed and
when it was last visited.
Additionally, it accounts for the type of each visit through a bonus system.

To account for `recency`, a bucketing system is implemented.
If a page has been visited later than the bucket cutoff, it gets the weight
associated with that bucket:

- Up to 4 days old - weight 100 - ``places.frecency.firstBucketCutoff/Weight``
- Up to 14 days old - weight 70 - ``places.frecency.secondBucketCutoff/Weight``
- Up to 31 days old - weight 50 - ``places.frecency.thirdBucketCutoff/Weight``
- Up to 90 days old - weight 30 - ``places.frecency.fourthBucketCutoff/Weight``
- Anything else - weight 10 - ``places.frecency.defaultBucketWeight``

To account for `frequency`, the total number of visits to a page is used to
calculate the final score.

The type of each visit is taken into account using specific bonuses:

Default bonus
  Any unknown type gets a default bonus. This is expected to be unused.
  Pref ``places.frecency.defaultVisitBonus`` current value: 0.
Embed
  Used for embedded/framed visits not due to user actions. These visits today
  are stored in memory and never participate to frecency calculation.
  Thus this is currently unused.
  Pref ``places.frecency.embedVisitBonus`` current value: 0.
Framed Link
  Used for cross-frame visits due to user action.
  Pref ``places.frecency.framedLinkVisitBonus`` current value: 0.
Download
  Used for download visits. It’s important to support link coloring for these
  visits, but they are not necessarily useful address bar results (the Downloads
  view can do a better job with  these), so their frecency can be low.
  Pref ``places.frecency.downloadVisitBonus`` current value: 0.
Reload
  Used for reload visits (refresh same page). Low because it should not be
  possible to influence frecency by multiple reloads.
  Pref ``places.frecency.reloadVisitBonus`` current value: 0.
Redirect Source
  Used when the page redirects to another one.
  It’s a low value because we give more importance to the final destination,
  that is what the user actually visits, especially for permanent redirects.
  Pref ``places.frecency.redirectSourceVisitBonus`` current value: 25.
Temporary Redirect
  Used for visits resulting from a temporary redirect (HTTP 307).
  Pref ``places.frecency.tempRedirectVisitBonus`` current value: 40.
Permanent Redirect
  Used for visits resulting from a permanent redirect (HTTP 301). This is the
  new supposed destination for a url, thus the bonus is higher than temporary.
  In this case it may be advisable to just pick the bonus for the source visit.
  Pref ``places.frecency.permRedirectVisitBonus`` current value: 50.
Bookmark
  Used for visits generated from bookmark views.
  Pref ``places.frecency.bookmarkVisitBonus`` current value: 75.
Link
  Used for normal visits, for example when clicking on a link.
  Pref ``places.frecency.linkVisitBonus`` current value: 100.
Typed
  Intended to be used for pages typed by the user, in reality it is used when
  the user picks a url from the UI (history views or the Address Bar).
  Pref ``places.frecency.typedVisitBonus`` current value: 2000.

The above bonuses are applied to visits, in addition to that there are also a
few bonuses applied in case a page is not visited at all, both of these bonuses
can be applied at the same time:

Unvisited bookmarked page
  Used for pages that are bookmarked but unvisited.
  Pref ``places.frecency.unvisitedBookmarkBonus`` current value: 140.
Unvisited typed page
  Used for pages that were typed and now are bookmarked (otherwise they would
  be orphans).
  Pref ``places.frecency.unvisitedTypedBonus`` current value: 200.

Two special frecency values are also defined:

- ``-1`` represents a just inserted entry in the database, whose score has not
  been calculated yet.
- ``0`` represents an entry for which a new value should not be calculated,
  because it has a poor user value (e.g. place: queries) among search results.

Finally, because calculating a score from all of the visits every time a new
visit is added would be expensive, only a sample of the last 10
(pref ``places.frecency.numVisits``) visits is used.

How frecency for a page is calculated
-------------------------------------

.. mermaid::
    :align: center
    :caption: Frecency calculation flow

    flowchart TD
        start[URL]
        a0{Has visits?}
        a1[Get last 10 visit]
        a2[bonus = unvisited_bonus + bookmarked + typed]
        a3{bonus > 0?}
        end0[Frecency = 0]
        end1["frecency = age_bucket_weight * (bonus / 100)"]
        a4[Sum points of all sampled visits]
        a5{points > 0?}
        end2[frecency = -1]
        end3["Frecency = visit_count * (points / sample_size)"]
        subgraph sub [Per each visit]
            sub0[bonus = visit_type_bonus]
            sub1{bookmarked?}
            sub2[add bookmark bonus]
            sub3["score = age_bucket_weight * (bonus / 100)"]
            sub0 --> sub1
            sub1 -- yes --> sub2
            sub1 -- no --> sub3
            sub2 --> sub3;
        end
        start --> a0
        a0 -- no --> a2
        a2 --> a3
        a3 -- no --> end0
        a3 -- yes --> end1
        a0 -- yes --> a1
        a1 --> sub
        sub --> a4
        a4 --> a5
        a5 -- no --> end2
        a5 -- yes --> end3

1. If the page is visited, get a sample of ``NUM_VISITS`` most recent visits.
2. For each visit get a transition bonus, depending on the visit type.
3. If the page is bookmarked, add to the bonus an additional bookmark bonus.
4. If the bonus is positive, get a bucket weight depending on the visit date.
5. Calculate points for the visit as ``age_bucket_weight * (bonus / 100)``.
6. Sum points for all the sampled visits.
7. If the points sum is zero, return a ``-1`` frecency, it will still appear in the UI.
   Otherwise, frecency is ``visitCount * points / NUM_VISITS``.
8. If the page is unvisited and not bookmarked, or it’s a bookmarked place-query,
   return a ``0`` frecency, to hide it from the UI.
9. If it’s bookmarked, add the bookmark bonus.
10. If it’s also a typed page, add the typed bonus.
11. Frecency is ``age_bucket_weight * (bonus / 100)``

When frecency for a page is calculated
--------------------------------------

Operations that may influence the frecency score are:

* Adding visits
* Removing visits
* Adding bookmarks
* Removing bookmarks
* Changing the url of a bookmark

Frecency is recalculated:

* Immediately, when a new visit is added. The user expectation here is that the
  page appears in search results after being visited. This is also valid for
  any History API that allows to add visits.
* In background on idle times, in any other case. In most cases having a
  temporarily stale value is not a problem, the main concern would be privacy
  when removing history of a page, but removing whole history will either
  completely remove the page or, if it's bookmarked, it will still be relevant.
  In this case, when a change influencing frecency happens, the ``recalc_frecency``
  database field for the page is set to ``1``.

Recalculation is done by the `PlacesFrecencyRecalculator <https://searchfox.org/mozilla-central/source/toolkit/components/places/PlacesFrecencyRecalculator.sys.mjs>`_ module.
The Recalculator is notified when ``PlacesUtils.history.shouldStartFrecencyRecalculation``
value changes from false to true, that means there's values to recalculate.
A DeferredTask is armed, that will look for a user idle opportunity
in the next 5 minutes, otherwise it will run when that time elapses.
Once all the outdated values have been recalculated
``PlacesUtils.history.shouldStartFrecencyRecalculation`` is set back to false
until the next operation invalidating a frecency.
The recalculation task is also armed on the ``idle-daily`` notification.

When the task is executed, it recalculates frecency of a chunk of pages. If
there are more pages left to recalculate, the task is re-armed. After frecency
of a page is recalculated, its ``recalc_frecency`` field is set back to ``0``.

Frecency is also decayed daily during the ``idle-daily`` notification, by
multiplying all the scores by a decay rate  of ``0.975`` (half-life of 28 days).
This guarantees entries not receiving new visits or bookmarks lose relevancy.


Adaptive Input History
======================

Input History (also known as Adaptive History) is a feature that allows to
find urls that the user previously picked. To do so, it associates search strings
with picked urls.

Adaptive results are usually presented before frecency derived results, making
them appear as having an infinite frecency.

When the user types a given string, and picks a result from the address bar, that
relation is stored and increases a use_count field for the given string.
The use_count field asymptotically approaches a max of ``10`` (the update is
done as ``use_count * .9 + 1``).

On querying, all the search strings that start with the input string are matched,
a rank is calculated per each page as ``ROUND(MAX(use_count) * (1 + (input = :search_string)), 1)``,
so that results perfectly matching the search string appear at the top.
Results with the same rank are additionally sorted by descending frecency.

On daily idles, when frecency is decayed, also input history gets decayed, in
particular the use_count field is multiplied by a decay rate  of ``0.975``.
After decaying, any entry that has a ``use_count < 0.975^90 (= 0.1)`` is removed,
thus entries are removed if unused for 90 days.
