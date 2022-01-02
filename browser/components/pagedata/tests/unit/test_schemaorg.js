/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the page data service can parse schema.org metadata.
 */

add_task(async function test_single_product_data() {
  await verifyPageData(
    `
      <html>
      <head>
      <title>Product Info 1</title>
      <meta http-equiv="Content-Type" content="text/html;charset=utf-8"></meta>
      </head>
      <body>
        <div itemscope itemtype="https://schema.org/Organization">
          <span itemprop="name">Mozilla</span>
        </div>

        <div itemscope itemtype="https://schema.org/Product">
          <img itemprop="image" src="bon-echo-microwave-17in.jpg" />
          <a href="microwave.html" itemprop="url">
            <span itemprop="name">Bon Echo Microwave</span>
          </a>

          <span itemprop="price" content="3.00">£3.00</span>
          <span itemprop="priceCurrency" content="GBP"></span>

          <span itemprop="gtin" content="13572468"></span>

          <span itemprop="description">The most amazing microwave in the world</span>
        </div>
      </body>
      </html>
    `,
    {
      siteName: "Mozilla",
      description: "The most amazing microwave in the world",
      image: BASE_URL + "/bon-echo-microwave-17in.jpg",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3,
            currency: "GBP",
          },
        },
      },
    }
  );
});

add_task(async function test_single_multiple_data() {
  await verifyPageData(
    `
      <html>
      <head>
      <title>Product Info 2</title>
      <meta http-equiv="Content-Type" content="text/html;charset=utf-8"></meta>
      </head>
      <body>
        <div itemscope itemtype="https://schema.org/Product">
          <img itemprop="image" src="bon-echo-microwave-17in.jpg" />
          <a href="microwave.html" itemprop="url">
            <span itemprop="name">Bon Echo Microwave</span>
          </a>

          <span itemprop="price" content="3.00">£3.00</span>
          <span itemprop="priceCurrency" content="GBP"></span>

          <span itemprop="gtin" content="13572468"></span>
        </div>
        <div itemscope itemtype="http://schema.org/Product">
          <img itemprop="image" src="gran-paradiso-toaster-17in.jpg" />
          <a href="toaster.html" itemprop="url">
            <span itemprop="name">Gran Paradiso Toaster</span>
          </a>

          <span itemprop="gtin" content="15263748"></span>
        </div>
      </body>
      </html>
    `,
    {
      image: BASE_URL + "/bon-echo-microwave-17in.jpg",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3,
            currency: "GBP",
          },
        },
      },
    }
  );
});
