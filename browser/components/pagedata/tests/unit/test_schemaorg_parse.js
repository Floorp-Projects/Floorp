/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the page data service can parse schema.org metadata into Item
 * structures.
 */

const { SchemaOrgPageData } = ChromeUtils.importESModule(
  "resource:///modules/pagedata/SchemaOrgPageData.sys.mjs"
);

/**
 * Collects the schema.org items from the given html string.
 *
 * @param {string} docStr
 *   The html to parse.
 * @returns {Promise<Item[]>}
 */
async function collectItems(docStr) {
  let doc = await parseDocument(docStr);
  return SchemaOrgPageData.collectItems(doc);
}

/**
 * Verifies that the items parsed from the html match the expected JSON-LD
 * format.
 *
 * @param {string} docStr
 *   The html to parse.
 * @param {object[]} expected
 *   The JSON-LD objects to match to.
 */
async function verifyItems(docStr, expected) {
  let items = await collectItems(docStr);
  let jsonLD = items.map(item => item.toJsonLD());
  Assert.deepEqual(jsonLD, expected);
}

add_task(async function test_microdata_parse() {
  await verifyItems(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <title>Product Info 1</title>
      </head>
      <body itemprop="badprop">
        <div itemscope itemtype="https://schema.org/Organization">
          <div itemprop="employee" itemscope itemtype="https://schema.org/Person">
            <span itemprop="name">Mr. Nested Name</span>
          </div>

          <span itemprop="name">Mozilla</span>
        </div>

        <div itemscope itemtype="https://schema.org/Product">
          <img itemprop="image" src="bon-echo-microwave-17in.jpg" />
          <a href="microwave.html" itemprop="url">
            <span itemprop="name">Bon Echo Microwave</span>
          </a>

          <div itemprop="offers" itemscope itemtype="https://schema.org/Offer">
            <span itemprop="price" content="3.50">£3.50</span>
            <span itemprop="priceCurrency" content="GBP"></span>
          </div>

          <span itemprop="gtin" content="13572468"></span>

          <span itemprop="description">The most amazing microwave in the world</span>
        </div>
      </body>
      </html>
    `,
    [
      {
        "@type": "Organization",
        employee: {
          "@type": "Person",
          name: "Mr. Nested Name",
        },
        name: "Mozilla",
      },
      {
        "@type": "Product",
        image: BASE_URL + "/bon-echo-microwave-17in.jpg",
        url: BASE_URL + "/microwave.html",
        name: "Bon Echo Microwave",
        offers: {
          "@type": "Offer",
          price: "3.50",
          priceCurrency: "GBP",
        },
        gtin: "13572468",
        description: "The most amazing microwave in the world",
      },
    ]
  );
});

add_task(async function test_json_ld_parse() {
  await verifyItems(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <script type="application/ld+json">
        {
          "@context": "http://schema.org",
          "@type": "Organization",
          "employee": {
            "@type": "Person",
            "name": "Mr. Nested Name"
          },
          "name": "Mozilla"
        }
      </script>
      <script type="application/ld+json">
        {
          "@context": "https://schema.org",
          "@type": "Product",
          "image": "bon-echo-microwave-17in.jpg",
          "url": "microwave.html",
          "name": "Bon Echo Microwave",
          "offers": {
            "@type": "Offer",
            "price": "3.50",
            "priceCurrency": "GBP"
          },
          "gtin": "13572468",
          "description": "The most amazing microwave in the world"
        }
      </script>
      </head>
      <body>
      </body>
      </html>
    `,
    [
      {
        "@type": "Organization",
        employee: {
          "@type": "Person",
          name: "Mr. Nested Name",
        },
        name: "Mozilla",
      },
      {
        "@type": "Product",
        image: "bon-echo-microwave-17in.jpg",
        url: "microwave.html",
        name: "Bon Echo Microwave",
        offers: {
          "@type": "Offer",
          price: "3.50",
          priceCurrency: "GBP",
        },
        gtin: "13572468",
        description: "The most amazing microwave in the world",
      },
    ]
  );
});

add_task(async function test_microdata_lazy_image() {
  await verifyItems(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <title>Product Info 1</title>
      </head>
      <body itemprop="badprop">
        <div itemscope itemtype="https://schema.org/Product">
          <img itemprop="image" src="lazy-load.gif" data-src="bon-echo-microwave-17in.jpg" />
          <a href="microwave.html" itemprop="url">
            <span itemprop="name">Bon Echo Microwave</span>
          </a>
        </div>
      </body>
      </html>
    `,
    [
      {
        "@type": "Product",
        image: BASE_URL + "/bon-echo-microwave-17in.jpg",
        url: BASE_URL + "/microwave.html",
        name: "Bon Echo Microwave",
      },
    ]
  );
});
