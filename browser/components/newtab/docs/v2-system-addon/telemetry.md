# Telemetry checklist

Adding telemetry generally involves a few steps:

1. File a "user story" bug about who wants what question answered. This will be used to track the client-side implementation as well as the data review request. If the server side changes are needed, ask Nan (:nanj / @ncloudio) if in doubt, bugs will be filed separately as dependencies.
1. Implement as usual...
1. Get review from Nan on the data schema and the documentation changes.
1. Request `data-review` of your documentation changes from a [data steward](https://wiki.mozilla.org/Firefox/Data_Collection) to ensure suitability for collection controlled by the opt-out `datareporting.healthreport.uploadEnabled` pref. Download and fill out the [data review request form](https://github.com/mozilla/data-review/blob/master/request.md) and then attach it as a text file on Bugzilla so you can r? a data steward. We've been working with Chris H-C (:chutten) for the Firefox specific telemetry, and Kenny Long (kenny@getpocket.com) for the Pocket specific telemetry, they are the best candidates for the review work as they know well about the context.
1. After landing the implementation, check with Nan to make sure the pings are making it to the database.
1. Once data flows in, you can build dashboard for the new telemetry on [Redash](https://sql.telemetry.mozilla.org/dashboards). If you're looking for some help about Redash or dashboard building, Nan is the guy for that.
