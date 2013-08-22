<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `places/bookmarks` module provides functions for creating, modifying and searching bookmark items. It exports:

* three constructors: [Bookmark](modules/sdk/places/bookmarks.html#Bookmark), [Group](modules/sdk/places/bookmarks.html#Group), and [Separator](modules/sdk/places/bookmarks.html#Separator), corresponding to the types of objects, referred to as **bookmark items**, in the Bookmarks database in Firefox
* two additional functions, [`save()`](modules/sdk/places/bookmarks.html#save(bookmarkItems%2C%20options)) to create, update, and remove bookmark items, and [`search()`](modules/sdk/places/bookmarks.html#search(queries%2C%20options)) to retrieve the bookmark items that match a particular set of criteria.

`save()` and `search()` are both asynchronous functions: they synchronously return a [`PlacesEmitter`](modules/sdk/places/bookmarks.html#PlacesEmitter) object, which then asynchronously emits events as the operation progresses and completes.

Each retrieved bookmark item represents only a snapshot of state at a specific time. The module does not automatically sync up a `Bookmark` instance with ongoing changes to that item in the database from the same add-on, other add-ons, or the user.

## Examples

### Creating a new bookmark

    let { Bookmark, save } = require("sdk/places/bookmarks");

    // Create a new bookmark instance, unsaved
    let bookmark = Bookmark({ title: "Mozilla", url: "http://mozila.org" });

    // Attempt to save the bookmark instance to the Bookmarks database
    // and store the emitter
    let emitter = save(bookmark);

    // Listen for events
    emitter.on("data", function (saved, inputItem) {
      // on a "data" event, an item has been updated, passing in the
      // latest snapshot from the server as `saved` (with properties
      // such as `updated` and `id`), as well as the initial input
      // item as `inputItem`
      console.log(saved.title === inputItem.title); // true
      console.log(saved !== inputItem); // true
      console.log(inputItem === bookmark); // true
    }).on("end", function (savedItems, inputItems) {
      // Similar to "data" events, except "end" is an aggregate of
      // all progress events, with ordered arrays as `savedItems`
      // and `inputItems`
    });

### Creating several bookmarks with a new group

    let { Bookmark, Group, save } = require("sdk/places/bookmarks");

    let group = Group({ title: "Guitars" });
    let bookmarks = [
      Bookmark({ title: "Ran", url: "http://ranguitars.com", group: group }),
      Bookmark({ title: "Ibanez", url: "http://ibanez.com", group: group }),
      Bookmark({ title: "ESP", url: "http://espguitars.com", group: group })
    ];

    // Save `bookmarks` array -- notice we don't have `group` in the array,
    // although it needs to be saved since all bookmarks are children
    // of `group`. This will be saved implicitly.

    save(bookmarks).on("data", function (saved, input) {
      // A data event is called once for each item saved, as well
      // as implicit items, like `group`
      console.log(input === group || ~bookmarks.indexOf(input)); // true
    }).on("end", function (saves, inputs) {
      // like the previous example, the "end" event returns an 
      // array of all of our updated saves. Only explicitly saved 
      // items are returned in this array -- the `group` won't be
      // present here.
      console.log(saves[0].title); // "Ran"
      console.log(saves[2].group.title); // "Guitars"
    });

### Searching for bookmarks

Bookmarks can be queried with the [`search()`](modules/sdk/places/bookmarks.html#search(queries%2C%20options)) function, which accepts a query object or an array of query objects, as well as a query options object. Query properties are AND'd together within a single query object, but are OR'd together across multiple query objects.

    let { search, UNSORTED } = require("sdk/places/bookmarks");

    // Simple query with one object
    search(
      { query: "firefox" },
      { sort: "title" }
    ).on(end, function (results) {
      // results matching any bookmark that has "firefox"
      // in its URL, title or tag, sorted by title
    });

    // Multiple queries are OR'd together
    search(
      [{ query: "firefox" }, { group: UNSORTED, tags: ["mozilla"] }],
      { sort: "title" }
    ).on("end", function (results) {
      // Our first query is the same as the simple query above;
      // all of those results are also returned here. Since multiple
      // queries are OR'd together, we also get bookmarks that
      // match the second query. The second query's properties
      // are AND'd together, so results that are in the platform's unsorted
      // bookmarks folder, AND are also tagged with 'mozilla', get returned
      // as well in this query
    });

<api name="Bookmark">
@class
<api name="Bookmark">
@constructor

Creates an unsaved bookmark instance.
@param options {object}
  Options for the bookmark, with the following parameters:
  @prop title {string}
    The title for the bookmark. Required.
  @prop url {string}
    The URL for the bookmark. Required.
  @prop [group] {Group}
    The parent group that the bookmark lives under. Defaults to the [Bookmarks.UNSORTED](modules/sdk/places/bookmarks.html#UNSORTED) group.
  @prop [index] {number}
    The index of the bookmark within its group. Last item within the group by default.
  @prop [tags] {set}
    A set of tags to be applied to the bookmark.
</api>

<api name="title">
@property {string}
  The bookmark's title.
</api>

<api name="url">
@property {string}
  The bookmark's URL.
</api>

<api name="group">
@property {Group}
  The group instance that the bookmark lives under.
</api>

<api name="index">
@property {number}
  The index of the bookmark within its group.
</api>

<api name="updated">
@property {number}
  A Unix timestamp indicating when the bookmark was last updated on the platform.
</api>

<api name="tags">
@property {set}
  A [Set](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set) of tags that the bookmark is tagged with.
</api>
</api>

<api name="Group">
@class
<api name="Group">
@constructor

Creates an unsaved bookmark group instance.
@param options {object}
  Options for the bookmark group, with the following parameters:
  @prop title {string}
    The title for the group. Required.
  @prop [group] {Group}
    The parent group that the bookmark group lives under. Defaults to the [Bookmarks.UNSORTED](modules/sdk/places/bookmarks.html#UNSORTED) group.
  @prop [index] {number}
    The index of the bookmark group within its parent group. Last item within the group by default.
</api>

<api name="title">
@property {string}
  The bookmark group's title.
</api>

<api name="group">
@property {Group}
  The group instance that the bookmark group lives under.
</api>

<api name="index">
@property {number}
  The index of the bookmark group within its group.
</api>

<api name="updated">
@property {number}
  A Unix timestamp indicating when the bookmark was last updated on the platform.
</api>
</api>

<api name="Separator">
@class
<api name="Separator">
@constructor

Creates an unsaved bookmark separator instance.
@param options {object}
  Options for the bookmark group, with the following parameters:
  @prop [group] {Group}
    The parent group that the bookmark group lives under. Defaults to the [Bookmarks.UNSORTED](modules/sdk/places/bookmarks.html#UNSORTED) group.
  @prop [index] {number}
    The index of the bookmark group within its parent group. Last item within the group by default.
</api>

<api name="group">
@property {Group}
  The group instance that the bookmark group lives under.
</api>

<api name="index">
@property {number}
  The index of the bookmark group within its group.
</api>

<api name="updated">
@property {number}
  A Unix timestamp indicating when the bookmark was last updated on the platform.
</api>
</api>

<api name="save">
@function

Creating, saving, and deleting bookmarks are all done with the `save()` function. This function takes in any of:

* a bookmark item (Bookmark, Group, Separator)
* a duck-typed object (the relative properties for a bookmark item, in addition to a `type` property of `'bookmark'`, `'group'`, or `'separator'`)
* an array of bookmark items.

All of the items passed in are pushed to the platform and are either created, updated or deleted.

* adding items: if passing in freshly instantiated bookmark items or a duck-typed object, the item is created on the platform.
* updating items: for an item referenced from a previous `save()` or from the result of a `search()` query, changing the properties and calling `save()` will update the item on the server.
* deleting items: to delete a bookmark item, pass in a bookmark item with a property `remove` set to `true`.

The function returns a [`PlacesEmitter`](modules/sdk/places/bookmarks.html#PlacesEmitter) that emits a `data` event for each item as it is saved, and an `end` event when all items have been saved.

    let { Bookmark, Group } = require("sdk/places/bookmarks");

    let myGroup = Group({ title: "My Group" });
    let myBookmark = Bookmark({
      title: "Moz",
      url: "http://mozilla.com",
      group: myGroup
    });

    save(myBookmark).on("data", function (item, inputItem) {
      // The `data` event returns the latest snapshot from the
      // host, so this is a new instance of the bookmark item,
      // where `item !== myBookmark`. To match it with the input item,
      // use the second argument, so `inputItem === myBookmark`

      // All explicitly saved items have data events called, as
      // well as implicitly saved items. In this case,
      // `myGroup` has to be saved before `myBookmark`, since
      // `myBookmark` is a child of `myGroup`. `myGroup` will
      // also have a `data` event called for it.
    }).on("end", function (items, inputItems) {
      // The `end` event returns all items that are explicitly
      // saved. So `myGroup` will not be in this array,
      // but `myBookmark` will be.
      // `inputItems` matches the initial input as an array,
      // so `inputItems[0] === myBookmark`
    });

    // Saving multiple bookmarks, as duck-types in this case

    let bookmarks = [
      { title: "mozilla", url: "http://mozilla.org", type: "bookmark" },
      { title: "firefox", url: "http://firefox.com", type: "bookmark" },
      { title: "twitter", url: "http://twitter.com", type: "bookmark" }
    ];

    save(bookmarks).on("data", function (item, inputItem) {
      // Each item in `bookmarks` has its own `data` event
    }).on("end", function (results, inputResults) {
      // `results` is an array of items saved in the same order
      // as they were passed in.
    });

@param bookmarkItems {bookmark|group|separator|array}
  A bookmark item ([Bookmark](modules/sdk/places/bookmarks.html#Bookmark), [Group](modules/sdk/places/bookmarks.html#Group), [Separator](modules/sdk/places/bookmarks.html#Separator)), or an array of bookmark items to be saved.

@param [options] {object}
  An optional options object that takes the following properties:
  @prop [resolve] {function}
    A resolution function that is invoked during an attempt to save
    a bookmark item that is not derived from the latest state from
    the platform. Invoked with two arguments, `mine` and `platform`, where
    `mine` is the item that is being saved, and `platform` is the
    current state of the item on the item. The object returned from
    this function is what is saved on the platform. By default, all changes
    on an outdated bookmark item overwrite the platform's bookmark item.

@returns {PlacesEmitter}
  Returns a [PlacesEmitter](modules/sdk/places/bookmarks.html#PlacesEmitter).
</api>

<api name="remove">
@function

A helper function that takes in a bookmark item, or an `Array` of several bookmark items, and sets each item's `remove` property to true. This does not remove the bookmark item from the database: it must be subsequently saved.

    let { search, save, remove } = require("sdk/places/bookmarks");

    search({ tags: ["php"] }).on("end", function (results) {
      // The search returns us all bookmark items that are
      // tagged with `"php"`.
      
      // We then pass `results` into the remove function to mark
      // all items to be removed, which returns the new modified `Array`
      // of items, which is passed into save.
      save(remove(results)).on("end", function (results) {
        // items tagged with `"php"` are now removed!
      });
    })

@param items {Bookmark|Group|Separator|array}
  A bookmark item, or `Array` of bookmark items to be transformed to set their `remove` property to `true`.

@returns {array}
  An array of the transformed bookmark items.
</api>

<api name="search">
@function

Queries can be performed on bookmark items by passing in one or more query objects, each of which is given one or more properties.

Within each query object, the properties are AND'd together: so only objects matching all properties are retrieved. Across query objects, the results are OR'd together, meaning that if an item matches any of the query objects, it will be retrieved.

For example, suppose we called `search()` with two query objects:

<pre>[{ url: "mozilla.org", tags: ["mobile"]},
 { tags: ["firefox-os"]}]</pre>

This will return:

* all bookmark items from mozilla.org that are also tagged "mobile"
* all bookmark items that are tagged "firefox-os"

An `options` object may be used to determine overall settings such as sort order and how many objects should be returned.

@param queries {object|array}
An `Object` representing a query, or an `Array` of `Objects` representing queries. Each query object can take several properties, which are queried against the bookmarks database. Each property is AND'd together, meaning that bookmarks must match each property within a query object. Multiple query objects are then OR'd together.
  @prop [group] {Group}
    Group instance that should be owners of the returned children bookmarks. If no `group` specified, all bookmarks are under the search space.
  @prop [tags] {set|array}
    Bookmarks with corresponding tags. These are AND'd together.
  @prop [url] {string}
    A string that matches bookmarks' URL. The following patterns are accepted:
    
    `'*.mozilla.com'`: matches any URL with 'mozilla.com' as the host, accepting any subhost.
    
    `'mozilla.com'`: matches any URL with 'mozilla.com' as the host.
    
    `'http://mozilla.com'`: matches 'http://mozilla.com' exactly.
    
    `'http://mozilla.com/*'`: matches any URL that starts with 'http://mozilla.com/'.
  @prop [query] {string}
    A string that matches bookmarks' URL, title and tags.

@param [options] {object}
An `Object` with options for the search query.
  @prop [count] {number}
    The number of bookmark items to return. If left undefined, no limit is set.
  @prop [sort] {string}
    A string specifying how the results should be sorted. Possible options are `'title'`, `'date'`, `'url'`, `'visitCount'`, `'dateAdded'` and `'lastModified'`.
  @prop [descending] {boolean}
    A boolean specifying whether the results should be in descending order. By default, results are in ascending order.

</api>

<api name="PlacesEmitter">
@class

The `PlacesEmitter` is not exported from the module, but returned from the `save` and `search` functions. The `PlacesEmitter` inherits from [`event/target`](modules/sdk/event/target.html), and emits `data`, `error`, and `end`.

`data` events are emitted for every individual operation (such as: each item saved, or each item found by a search query), whereas `end` events are emitted as the aggregate of an operation, passing an array of objects into the handler.

<api name="data">
@event
The `data` event is emitted when a bookmark item that was passed into the `save` method has been saved to the platform. This includes implicit saves that are dependencies of the explicit items saved. For example, when creating a new bookmark group with two bookmark items as its children, and explicitly saving the two bookmark children, the unsaved parent group will also emit a `data` event.

    let { Bookmark, Group, save } = require("sdk/places/bookmarks");

    let group = Group({ title: "my group" });
    let bookmarks = [
      Bookmark({ title: "mozilla", url: "http://mozilla.com", group: group }),
      Bookmark({ title: "w3", url: "http://w3.org", group: group })
    ];

    save(bookmarks).on("data", function (item) {
      // This function will be called three times:
      // once for each bookmark saved
      // once for the new group specified implicitly
      // as the parent of the two items
    });

The `data` event is also called for `search` requests, with each result being passed individually into its own `data` event.

    let { search } = require("sdk/places/bookmarks");

    search({ query: "firefox" }).on("data", function (item) {
      // each bookmark item that matches the query will 
      // be called in this function
    });

@argument {Bookmark|Group|Separator}
  For the `save` function, this is the saved, latest snapshot of the bookmark item. For `search`, this is a snapshot of a bookmark returned from the search query.

@argument {Bookmark|Group|Separator|object}
  Only in `save` data events. The initial instance of the item that was used for the save request.
</api>

<api name="error">
@event
The `error` event is emitted whenever a bookmark item's save could not be completed.

@argument {string}
  A string indicating the error that occurred.

@argument {Bookmark|Group|Separator|object}
  Only in `save` error events. The initial instance of the item that was used for the save request.
</api>

<api name="end">
@event
The `end` event is called when all bookmark items and dependencies
have been saved, or an aggregate of all items returned from a search query.

@argument {array}
  The array is an ordered list of the input bookmark items, replaced
  with their updated, latest snapshot instances (the first argument
  in the `data` handler), or in the case of an error, the initial instance
  of the item that was used for the save request
  (the second argument in the `data` or `error` handler).
</api>
</api>

<api name="MENU">
@property {group}
This is a constant, default [`Group`](modules/sdk/places/bookmarks.html#Group) on the Firefox platform, the **Bookmarks Menu**. It can be used in queries or specifying the parent of a bookmark item, but it cannot be modified.
</api>

<api name="TOOLBAR">
@property {group}
This is a constant, default [`Group`](modules/sdk/places/bookmarks.html#Group) on the Firefox platform, the **Bookmarks Toolbar**. It can be used in queries or specifying the parent of a bookmark item, but it cannot be modified.
</api>

<api name="UNSORTED">
@property {group}
This is a constant, default [`Group`](modules/sdk/places/bookmarks.html#Group) on the Firefox platform, the **Unsorted Bookmarks** group. It can be used in queries or specifying the parent of a bookmark item, but it cannot be modified.
</api>
