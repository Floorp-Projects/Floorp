# PageDataService

The page data service is responsible for collecting additional data about a page. This could include
information about the media on a page, product information, etc. When enabled it will automatically
try to find page data for pages that the user browses or it can be directed to asynchronously look
up the page data for a url.

The `PageDataService` is an EventEmitter and listeners can subscribe to its notifications via the
`on` and `once` methods.

The service can be enabled by setting `browser.pagedata.enabled` to true. Additional logging can be
enabled by setting `browser.pagedata.log` to true.

## PageData Data Structure

At a high level the page data service can collect many different kinds of data. When queried the
service will respond with a `PageData` structure which holds the page's url the time when the data
was discovered and an array of the different types of data found. This array will be empty if no
data was found. Each item in the array is an object with two properties. `type` is a string
indicating the type of data. `data` is the actual data, the specific format will vary depending on
the type.

## PageData Collection

Page data is gathered in one of two ways.

Page data is automatically gathered for webpages the user visits. This collection is trigged after
a short delay and then updated when necessary. Any data is cached in memory for a period of time.
When page data has been found a `page-data` event is emitted. The event's argument holds the
`PageData` structure. The `getCached` function can be used to access any cached data for a url.

## Supported Types of page data

The following types of page data (`PageDataCollector.DATA_TYPE`) are currently supported:

- [`PRODUCT`](./schema-org.html#Product)
