========================
Installation Attribution
========================

---------------
Microsoft Store
---------------

Firefox installs done through the Microsoft Store support extracting campaign IDs that may be embedded into them. This allows us to attribute installs through different channels by providing particular links to the Microsoft Store with attribution data included. For example:

`ms-windows-store://pdp/?productid=9NZVDKPMR9RD&cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set) <ms-windows-store://pdp/?productid=9NZVDKPMR9RD&cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)>`_


`https://www.microsoft.com/store/apps/9NZVDKPMR9RD?cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set) <https://www.microsoft.com/store/apps/9NZVDKPMR9RD?cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)>`_


For more on how custom campaign IDs work in general in the Microsoft Store environment, `see Microsoft's documentation <https://docs.microsoft.com/en-us/windows/uwp/publish/create-a-custom-app-promotion-campaign>`_.

The Microsoft Store provides a single `cid` (Campaign ID). Their documentation claims it is limited to 100 characters, although in our own testing we've been able to retrieve the first 208 characters of Campaign IDs. Firefox expects this Campaign ID to follow the same format as stub and full installer attribution codes, which have a maximum of length of 1010 characters. Since Campaign IDs are more limited than what Firefox allows, we need to be a bit more thoughtful about what we include in them vs. stub and full installer attribution. At the time of writing, we've yet to be able to test whether we can reliably pull more than the advertised 100 characters of a Campaign ID in the real world -- something that we should do before we send any crucial information past the first 100 characters.

In addition to the attribution data retrieved through the campaign ID, we also add an extra key to it to indicate whether or not the user was signed into the Microsoft Store when they installed. This `msstoresignedin` key can have a value of `true` or `false`.

There are a couple of other caveats to keep in mind:

* A campaign ID is only set the *first* time a user installs Firefox through the Store. Subsequent installs will inherit the original campaign ID (even if it was an empty string). This means that only brand new installs will be attributed -- not reinstalls.
* At the time of writing, it is not clear whether or not installs done without being signed into the Microsoft Store will be able to find their campaign ID. Microsoft's documentation claims they can, but our own testing has not been able to verify this.
