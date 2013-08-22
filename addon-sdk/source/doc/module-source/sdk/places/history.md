<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `places/history` module provides a single function, [`search()`](modules/sdk/places/history.html#search(queries%2C%20options)), for querying the user's browsing history.

It synchronously returns a [`PlacesEmitter`](modules/sdk/places/history.html#PlacesEmitter) object which then asynchronously emits [`data`](modules/sdk/places/history.html#data) and [`end`](modules/sdk/places/history.html#end) or [`error`](modules/sdk/places/history.html#error) events that contain information about the state of the operation.

## Example

    let { search } = require("sdk/places/history");

    // Simple query
    search(
      { url: "https://developers.mozilla.org/*" },
      { sort: "visitCount" }
    ).on("end", function (results) {
      // results is an array of objects containing
      // data about visits to any site on developers.mozilla.org
      // ordered by visit count
    });

    // Complex query
    // The query objects are OR'd together
    // Let's say we want to retrieve all visits from before a week ago
    // with the query of 'ruby', but from last week onwards, we want
    // all results with 'javascript' in the URL or title.
    // We'd compose the query with the following options
    let lastWeek = Date.now - (1000*60*60*24*7);
    search(
      // First query looks for all entries before last week with 'ruby'
      [{ query: "ruby", to: lastWeek },
      // Second query searches all entries after last week with 'javascript'
       { query: "javascript", from: lastWeek }],
      // We want to order chronologically by visit date
      { sort: "date" }
    ).on("end", function (results) {
      // results is an array of objects containing visit data,
      // sorted by visit date, with all entries from more than a week ago
      // that contain 'ruby', *in addition to* entries from this last week
      // that contain 'javascript'
    });

<api name="search">
@function

Queries can be performed on history entries by passing in one or more query options. Each query option can take several properties, which are **AND**'d together to make one complete query. For additional queries within the query, passing more query options in will **OR** the total results. An `options` object may be specified to determine overall settings, like sorting and how many objects should be returned.

@param queries {object|array}
An `Object` representing a query, or an `Array` of `Objects` representing queries. Each query object can take several properties, which are queried against the history database. Each property is **AND**'d together, meaning that bookmarks must match each property within a query object. Multiple query objects are then **OR**'d together.
  @prop [url] {string}
    A string that matches bookmarks' URL. The following patterns are accepted:
    
    `'*.mozilla.com'`: matches any URL with 'mozilla.com' as the host, accepting any subhost.
    
    `'mozilla.com'`: matches any URL with 'mozilla.com' as the host.
    
    `'http://mozilla.com'`: matches 'http://mozilla.com' directlry.
    
    `'http://mozilla.com/*'`: matches any URL that starts with 'http://mozilla.com/'.
  @prop [query] {string}
    A string that matches bookmarks' URL, or title.
  @prop [from] {number|date}
    Time relative from the [Unix epoch](http://en.wikipedia.org/wiki/Unix_time) that history results should be limited to occuring after. Can accept a [`Date`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date) object, or milliseconds from the epoch. Default is to return all items since the epoch (all time).
  @prop [to] {number|date}
    Time relative from the [Unix epoch](http://en.wikipedia.org/wiki/Unix_time) that history results should be limited to occuring before. Can accept a [`Date`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date) object, or milliseconds from the epoch. Default is the current time.

@param [options] {object}
An `Object` with options for the search query.
  @prop [count] {number}
    The number of bookmark items to return. If left undefined, no limit is set.
  @prop [sort] {string}
    A string specifying how the results should be sorted. Possible options are `'title'`, `'date'`, `'url'`, `'visitCount'`, `'keyword'`, `'dateAdded'` and `'lastModified'`.
  @prop [descending] {boolean}
    A boolean specifying whether the results should be in descending order. By default, results are in ascending order.

</api>


<api name="PlacesEmitter">
@class

The `PlacesEmitter` is not exposed in the module, but returned from the `search` functions. The `PlacesEmitter` inherits from [`event/target`](modules/sdk/event/target.html), and emits `data`, `error`, and `end`. `data` events are emitted for every individual search result found, whereas `end` events are emitted as an aggregate of an entire search, passing in an array of all results into the handler.

<api name="data">
@event
The `data` event is emitted for every item returned from a search.

@argument {Object}
  This is an object representing a history entry. Contains `url`, `time`, `accessCount` and `title` of the entry.
</api>

<api name="error">
@event
The `error` event is emitted whenever a search could not be completed.

@argument {string}
  A string indicating the error that occurred.
</api>

<api name="end">
@event
The `end` event is called when all search results have returned.

@argument {array}
  The value passed into the handler is an array of all entries found in the
  history search. Each entry is an object containing the properties
  `url`, `time`, `accessCount` and `title`.
</api>
</api>
