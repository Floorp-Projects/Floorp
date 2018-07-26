# Adding/Changing Telemetry Checklist

Adding telemetry generally involves a few steps:

1. - [ ] File a "user story issue" [in ping-centre](https://github.com/mozilla/ping-centre) about who wants what question answered.  This will be used to track server-side data handling implementation (ETL, dashboard implementation...).

    > As an engineer, I see the distribution of times between triggering a new tab load by tab menu entry, plus button or keyboard shortcut and when the visibility event is fired on that page, so that I can understand how long it takes before the page becomes visible and how close to instantaneous that feels.

1. - [ ] File or update your existing client-side implementation bug so that it points to the "user story issue".
1. - [ ] Talk to Marina (@emtwo) about the dashboard you're hoping to see from the data you're adding, and work with her to create a data model (or request her review on it after the fact).
1. - [ ] Update `system-addon/test/schemas/pings.js` with a commented JOI schema of your changes, and add tests to system-addon/test/unit/TelemetryFeed.test.js to exercise the ping creation.
1. - [ ] Update [data_events.md](data_events.md) with an example of the data in question
1. - [ ] Update any fields that you've added, deleted, or changed in [data_dictionary.md](data_dictionary.md)
1. - [ ] Get review from Nan (@ncloudioj) and/or Marina (@emtwo) on the data schema and the documentation changes.
1. - [ ] Request `data-review` of your documentation changes from a [data steward](https://wiki.mozilla.org/Firefox/Data_Collection) to ensure suitability for collection controlled by the opt-out `datareporting.healthreport.uploadEnabled` pref. Download and fill out the [data review request form](https://github.com/mozilla/data-review/blob/master/request.md) and then attach it as a text file on Bugzilla so you can r? a data steward. We've been working with François Marier (:francois / @fmarier), so he's a good candidate as he's likely to have the most context.
1. - [ ] Implement as usual...
1. - [ ] After landing the implementation, check with Nan (@ncloudioj) to make sure the pings are making it to the database
1. - [ ] Update the [ping-centre](https://github.com/mozilla/ping-centre/issues) user story issue for the dashboard, stating that the data should now be available for implementation, and removing any `blocked` tag if appropriate.

