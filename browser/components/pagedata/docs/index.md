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
service will respond with a `PageData` structure which holds some general information about the
page, the time when the data was discovered and a map of the different types of data found. This map
will be empty if no specific data was found. The key of the map is from the
`PageDataSchema.DATA_TYPE` enumeration. The value is the JSON data which differs in structure
depending on the data type.

```
{
  "url": <url of the page as a string>,
  "date": <epoch based timestamp>,
  "siteName": <a friendly name for the website>,
  "image": <url for an image for the page as a string>,
  "data": <map of data types>,
}
```

## PageData Collection

Page data is gathered in one of two ways.

Page data is automatically gathered for webpages the user visits. This collection is trigged after
a short delay and then updated when necessary. Any data is cached in memory for a period of time.
When page data has been found a `page-data` event is emitted. The event's argument holds the
`PageData` structure. The `getCached` function can be used to access any cached data for a url.

## Supported Types of page data

The following types of page data (`PageDataSchema.DATA_TYPE`) are currently supported:

- `PRODUCT`
- `DOCUMENT`
- `ARTICLE`
- `AUDIO`
- `VIDEO`
