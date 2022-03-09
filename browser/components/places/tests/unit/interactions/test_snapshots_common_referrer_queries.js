/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests of queries on common referrer snapshots (i.e. those with a referrer common to the context url's interactions)
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
});

/**
 * Reset all interactions and snapshots
 *
 */
async function reset_interactions_snapshots() {
  await Interactions.reset();
  await Snapshots.reset();
}

/**
 * Creates an interaction and snapshot for a given url
 *
 * @param {object} [options]
 * @param {string} [options.url]
 *   The url
 * @param {string} [options.referrer]
 *   The referrer url
 */
async function create_interaction_and_snapshot({
  url = "https://example.com/",
  referrer = "",
} = {}) {
  await addInteractions([
    {
      url,
      referrer,
    },
  ]);

  await Snapshots.add({ url });
}

// Neither visits came from a referrer, so no snapshots should be found
add_task(async function test_query_no_referrer() {
  await reset_interactions_snapshots();

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_a",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_b",
  });
});

// The interactions came from different referrers, so no snapshots should be found
add_task(async function test_query_no_common_referrer() {
  await reset_interactions_snapshots();

  // Add interactions for the referrer urls so they are in the places db
  await addInteractions([
    {
      url: "https://example.com/product_search",
    },
    {
      url: "https://example.com/",
    },
  ]);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
    referrer: "https://example.com/product_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
    referrer: "https://example.com",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_a",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_b",
  });
});

// The snapshots have interactions with a referrer common to the context url's interactions, so they should be found
add_task(async function test_query_common_referrer() {
  await reset_interactions_snapshots();

  // Add interactions for the referrer url
  await addInteractions([
    {
      url: "https://example.com/product_search",
    },
  ]);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
    referrer: "https://example.com/product_search",
  });

  // The context url should never be a self-suggestion
  let context = {
    url: "https://example.com/product_a",
  };
  await assertCommonReferrerSnapshots([], context);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
    referrer: "https://example.com/product_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_c",
    referrer: "https://example.com/product_search",
  });

  context = {
    url: "https://example.com/product_a",
  };
  await assertCommonReferrerSnapshots(
    [
      { url: "https://example.com/product_b", commonReferrerScoreEqualTo: 1.0 },
      { url: "https://example.com/product_c", commonReferrerScoreEqualTo: 1.0 },
    ],
    context
  );

  context = {
    url: "https://example.com/product_b",
  };
  await assertCommonReferrerSnapshots(
    [
      { url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 },
      { url: "https://example.com/product_c", commonReferrerScoreEqualTo: 1.0 },
    ],
    context
  );

  context = {
    url: "https://example.com/product_c",
  };
  await assertCommonReferrerSnapshots(
    [
      { url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 },
      { url: "https://example.com/product_b", commonReferrerScoreEqualTo: 1.0 },
    ],
    context
  );

  // The root search context should not lead to any suggestions
  context = {
    url: "https://example.com/product_search",
  };
  await assertCommonReferrerSnapshots([], context);
});

// Verify that common referrers are correctly found in a hierarchical browsing pattern
add_task(async function test_query_common_referrer_hierarchical() {
  await reset_interactions_snapshots();

  await create_interaction_and_snapshot({
    url: "https://example.com/top_level_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_search",
    referrer: "https://example.com/top_level_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/services_search",
    referrer: "https://example.com/top_level_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
    referrer: "https://example.com/product_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
    referrer: "https://example.com/product_search",
  });

  // With the top level search as context, no sites should be suggested
  let context = {
    url: "https://example.com/top_level_search",
  };

  await assertCommonReferrerSnapshots([], context);

  // With the children urls, only siblings should be found
  context = {
    url: "https://example.com/services_search",
  };
  await assertCommonReferrerSnapshots(
    [
      {
        url: "https://example.com/product_search",
        commonReferrerScoreEqualTo: 1.0,
      },
    ],
    context
  );

  context = {
    url: "https://example.com/product_search",
  };
  await assertCommonReferrerSnapshots(
    [
      {
        url: "https://example.com/services_search",
        commonReferrerScoreEqualTo: 1.0,
      },
    ],
    context
  );

  // And similarly for the leaf nodes
  context = {
    url: "https://example.com/product_a",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_b", commonReferrerScoreEqualTo: 1.0 }],
    context
  );
  context = {
    url: "https://example.com/product_b",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 }],
    context
  );
});

// The snapshots have multiple interactions, each with different referrers.
// Snapshots should be selected when the context referrer is common to one of the interactions.
add_task(async function test_query_common_referrer_multiple_interaction() {
  await reset_interactions_snapshots();

  // Add interactions for the referrer urls so they are in the places db
  await addInteractions([
    {
      url: "https://example.com/product_search",
    },
    {
      url: "https://example.com/top_level_search",
    },
  ]);

  // Add interactions with separate referrers
  await addInteractions([
    {
      url: "https://example.com/product_a",
      referrer: "https://example.com/product_search",
      created_at: Date.now(),
    },
    {
      url: "https://example.com/product_a",
      referrer: "https://example.com/top_level_search",
      created_at: Date.now() + 60000, // we can't have two interactions for the same url at the same time
    },
  ]);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
  });

  // Add an interaction with a single referrers
  await addInteractions([
    {
      url: "https://example.com/product_b",
      referrer: "https://example.com/product_search",
    },
  ]);
  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
  });

  // https://example.com/product_a should be found as a snapshot with either of the referrer urls
  let context = {
    url: "https://example.com/product_b",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 }],
    context
  );

  context = {
    url: "https://example.com/product_b",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 }],
    context
  );

  // No snapshot should be found if an unrelated referrer is used
  await addInteractions([
    {
      url: "https://example.com/product_c",
      referrer: "https://example.com/unrelated_search",
    },
  ]);

  context = {
    url: "https://example.com/product_c",
  };
  await assertCommonReferrerSnapshots([], context);
});

// The snapshots have multiple interactions and the page data is correctly retrieved
add_task(
  async function test_query_common_referrer_multiple_interactions_page_data() {
    await reset_interactions_snapshots();

    // Register some page data.
    PageDataService.pageDataDiscovered({
      url: "https://example.com/product_a",
      date: Date.now(),
      siteName: "Example site name",
      description: "Example site description",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          price: {
            value: 276,
            currency: "USD",
          },
        },
      },
    });

    // Add an interactions for the referrer url so it's in the places db
    await addInteractions([
      {
        url: "https://example.com/product_search",
      },
    ]);

    // Add multiple interactions with the same referrer
    await addInteractions([
      {
        url: "https://example.com/product_a",
        referrer: "https://example.com/product_search",
        created_at: Date.now(),
      },
      {
        url: "https://example.com/product_a",
        referrer: "https://example.com/product_search",
        created_at: Date.now() + 60000, // we can't have two interactions for the same url at the same time
      },
      {
        url: "https://example.com/product_a",
        referrer: "https://example.com/product_search",
        created_at: Date.now() + 120000, // we can't have two interactions for the same url at the same time
      },
    ]);

    await create_interaction_and_snapshot({
      url: "https://example.com/product_a",
    });

    // Add an interaction with a single referrers
    await addInteractions([
      {
        url: "https://example.com/product_b",
        referrer: "https://example.com/product_search",
      },
    ]);
    await create_interaction_and_snapshot({
      url: "https://example.com/product_b",
    });

    let context = {
      url: "https://example.com/product_b",
    };

    let snapshot = await Snapshots.queryCommonReferrer(context.url);

    Assert.equal(snapshot.length, 1, "One shapshot should be found");
    Assert.equal(
      snapshot[0].url,
      "https://example.com/product_a",
      "Correct snapshot should be found"
    );
    Assert.equal(
      snapshot[0].siteName,
      "Example site name",
      "Site name should be found"
    );
    Assert.equal(
      snapshot[0].description,
      "Example site description",
      "Site description should be found"
    );
    Assert.equal(
      snapshot[0].pageData.size,
      1,
      "Should have 1 item of page data."
    );
    Assert.deepEqual(
      snapshot[0].pageData.get(PageDataSchema.DATA_TYPE.PRODUCT),
      { price: { value: 276, currency: "USD" } },
      "Should have the right price."
    );
  }
);
