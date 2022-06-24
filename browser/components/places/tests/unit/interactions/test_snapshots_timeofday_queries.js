/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for queries on time of day snapshots (i.e. those whose interactions
 * happened at about the same time on previous days)
 */

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "snapshot_timeofday_interval_seconds",
  "browser.places.interactions.snapshotTimeOfDayIntervalSeconds",
  3600
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "snapshot_timeofday_expected_interactions",
  "browser.places.interactions.snapshotTimeOfDayExpectedInteractions",
  15
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "snapshot_timeofday_sessiondecay_start_s",
  "browser.places.interactions.snapshotTimeOfDaySessionDecayStartSeconds",
  1800 // 30 mins
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "snapshot_timeofday_sessiondecay_end_s",
  "browser.places.interactions.snapshotTimeOfDaySessionDecayEndSeconds",
  28800 // 8 hours
);

const now = new Date();

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.places.interactions.enabled", true);
  info("Sanity check: add random non-snapshot pages at 15 minutes intervals.");
  let nonSnapshotPages = [];
  for (let i = 7 * 24 * 4; i >= 0; --i) {
    nonSnapshotPages.push({
      uri: `https://nonsnapshot${i}.com/`,
      visitDate: PlacesUtils.toPRTime(now - i * 15 * 60 * 60 * 1000),
    });
  }
  await PlacesTestUtils.addVisits(nonSnapshotPages);
});

add_task(async function test_query() {
  await reset();

  // Made up so max / 2 is > snapshot_timeofday_expected_interactions.
  const interactionCounts = {
    min: 1,
    max: snapshot_timeofday_expected_interactions * 3,
  };
  Assert.greater(
    interactionCounts.max / 2,
    snapshot_timeofday_expected_interactions,
    "Half of max interactions is greater than expected interactions"
  );

  // Add snapshots in the interval and out of it.
  let snapshots = [
    {
      // We'll query with this as current uri, thus it should not be returned.
      url: "https://snapshot1.com/",
      created_at: new Date(now).setDate(now.getDate() - 1),
    },
    {
      url: "https://snapshot2.com/",
      // It's in interval, but for today.
      created_at: new Date(now).setMinutes(
        now.getMinutes() - snapshot_timeofday_interval_seconds / 60 / 4
      ),
    },
    {
      url: "https://snapshot3.com/",
      // It's in interval for yesterday.
      created_at: new Date(now).setDate(now.getDate() - 1),
    },
    {
      url: "https://snapshot4.com/",
      // It's in interval for 3 days ago with max interactions.
      created_at: new Date(now).setDate(now.getDate() - 3),
      interactionCount: interactionCounts.max,
    },
    {
      url: "https://snapshot5.com/",
      // It's out of interval for yesterday.
      created_at: new Date(new Date(now).setDate(now.getDate() - 1)).setMinutes(
        now.getMinutes() - snapshot_timeofday_interval_seconds / 60
      ),
    },
    {
      url: "https://snapshot6.com/",
      // It's in interval for a week ago.
      created_at: new Date(now).setDate(now.getDate() - 7),
    },
    {
      url: "https://snapshot7.com/",
      // It's out of interval for a week ago.
      created_at: new Date(new Date(now).setDate(now.getDate() - 7)).setMinutes(
        now.getMinutes() - snapshot_timeofday_interval_seconds / 60
      ),
    },
    {
      url: "https://snapshot8.com/",
      // It's in interval but a long time ago.
      created_at: new Date(now).setDate(now.getDate() - 100),
    },
    {
      url: "https://snapshot9.com/",
      // It's in interval for a week ago, with (maxInteractions / 2)
      // interactions.
      created_at: new Date(now).setDate(now.getDate() - 7),
      interactionCount: interactionCounts.max / 2,
    },
    {
      url: "https://snapshot10.com/",
      // It's in interval for a week ago, with less interactions than
      // (maxInteractions / 2), but at least snapshot_timeofday_expected_interactions.
      created_at: new Date(now).setDate(now.getDate() - 7),
      interactionCount: snapshot_timeofday_expected_interactions,
    },
  ];

  await addInteractions(snapshots);
  for (let snapshot of snapshots) {
    await Snapshots.add(snapshot);

    if (snapshot.interactionCount) {
      // Add more interactions if requested.
      let t = 0;
      let additionalInteractions = new Array(
        Math.round(snapshot.interactionCount) - 1 // One was already added.
      )
        .fill(snapshot.url)
        .map(url => ({
          url,
          created_at: new Date(
            new Date(now).setDate(now.getDate() - 4)
          ).setSeconds(++t),
        }));
      await addInteractions(additionalInteractions);
    }
  }

  // Also add an interaction far in the past and check it doesn't count for
  // score.
  await addInteractions([
    {
      url: "https://snapshot6.com/",
      created_at: new Date(now).setDate(now.getDate() - 100),
    },
  ]);

  // We are adding interactions quite in the past.
  const daysThreshold = 101;
  const context = {
    url: snapshots[0].url,
    time: now.getTime(),
  };
  await assertTimeOfDaySnapshots(
    [
      {
        url: "https://snapshot3.com/",
        scoreEqualTo: Snapshots.timeOfDayScore({
          interactions: 1,
          context,
          ...interactionCounts,
        }),
        daysThreshold,
        scoreLessThan: 1.0,
      },
      {
        url: "https://snapshot4.com/",
        scoreEqualTo: 1.0,
        daysThreshold,
      },
      {
        url: "https://snapshot6.com/",
        scoreEqualTo: Snapshots.timeOfDayScore({
          interactions: 1,
          context,
          ...interactionCounts,
        }),
        daysThreshold,
        scoreLessThan: 1.0,
      },
      {
        url: "https://snapshot9.com/",
        scoreEqualTo: 1.0,
        daysThreshold,
      },
      {
        url: "https://snapshot10.com/",
        scoreEqualTo: Snapshots.timeOfDayScore({
          interactions: snapshot_timeofday_expected_interactions,
          context,
          ...interactionCounts,
        }),
        daysThreshold,
        scoreLessThan: 1.0,
      },
    ],
    context
  );
});

add_task(async function test_session_decay() {
  await reset();
  Services.prefs.setIntPref(
    "browser.places.interactions.snapshotTimeOfDayExpectedInteractions",
    1
  );

  // Add snapshots in the interval and out of it.
  let snapshots = [
    {
      url: "https://snapshot3.com/",
      // It's in interval for yesterday.
      created_at: new Date(now).setDate(now.getDate() - 1),
    },
    {
      url: "https://snapshot4.com/",
      // It's in interval for 3 days ago with max interactions.
      created_at: new Date(now).setDate(now.getDate() - 3),
    },
    {
      url: "https://snapshot6.com/",
      // It's in interval for a week ago.
      created_at: new Date(now).setDate(now.getDate() - 7),
    },
  ];

  await addInteractions(snapshots);
  for (let snapshot of snapshots) {
    await Snapshots.add(snapshot);
  }

  const daysThreshold = 8;

  info("early in the session, before decay start");
  await assertTimeOfDaySnapshots(
    [
      {
        url: "https://snapshot3.com/",
        scoreEqualTo: 1.0,
        daysThreshold,
      },
      {
        url: "https://snapshot4.com/",
        scoreEqualTo: 1.0,
        daysThreshold,
      },
      {
        url: "https://snapshot6.com/",
        scoreEqualTo: 1.0,
        daysThreshold,
      },
    ],
    {
      url: "about:robots",
      time: now.getTime(),
      sessionStartTime:
        new Date(now) - snapshot_timeofday_sessiondecay_start_s * 1000,
    }
  );

  info("late in the session, after decay end");
  await assertTimeOfDaySnapshots(
    [
      {
        url: "https://snapshot3.com/",
        scoreEqualTo: 0.1,
        daysThreshold,
      },
      {
        url: "https://snapshot4.com/",
        scoreEqualTo: 0.1,
        daysThreshold,
      },
      {
        url: "https://snapshot6.com/",
        scoreEqualTo: 0.1,
        daysThreshold,
      },
    ],
    {
      url: "about:robots",
      time: now.getTime(),
      sessionStartTime:
        new Date(now) - snapshot_timeofday_sessiondecay_end_s * 1000 - 1000,
    }
  );

  info("between decay start and end");
  await assertTimeOfDaySnapshots(
    [
      {
        url: "https://snapshot3.com/",
        scoreLessThan: 1.0,
        scoreGreaterThan: 0.1,
        daysThreshold,
      },
      {
        url: "https://snapshot4.com/",
        scoreLessThan: 1.0,
        scoreGreaterThan: 0.1,
        daysThreshold,
      },
      {
        url: "https://snapshot6.com/",
        scoreLessThan: 1.0,
        scoreGreaterThan: 0.1,
        daysThreshold,
      },
    ],
    {
      url: "about:robots",
      time: now.getTime(),
      sessionStartTime:
        now.getTime() -
        (snapshot_timeofday_sessiondecay_start_s +
          (snapshot_timeofday_sessiondecay_end_s -
            snapshot_timeofday_sessiondecay_start_s) /
            2) *
          1000,
    }
  );
});
