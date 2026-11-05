When a generated Linux PWA launcher starts the process first, its `--name` becomes the process-wide GTK identity. PR #3032 assigns each PWA window its own launcher identity, but later ordinary browser windows still inherit the first PWA's Wayland app_id.

Register a synchronous existing pre-layout category only for an initial Linux `--start-ssb` command. In that process, ordinary windows without a window identity receive the canonical application remoting name. Taskbar/PWA windows and existing explicit classes are preserved. Browser-first processes, including their custom `--name`, do not register the callback. Old PWA launchers keep working without rewriting profile or desktop data. No native taskbar gate, Runtime lock or publication behavior changes.

This Draft depends on #3032 and targets its branch so the review contains only this follow-up. Related to KAN-49 / #2649; it does not claim original Manjaro/KDE/Outlook/Teams qualification.

Validation: frozen dependencies, type/lint/format checks, and all four actual-browser SsbCommandLineHandler regression tests pass, including the new real ordinary-window/PWA pre-layout test. The quick candidate is the completed #3032 package with source-generated ESM resources replaced; native files are unchanged, and it is **not** a new full package. Native Wayland order/migration comparisons and an exact-head complete production package run are in progress; results and provenance will be added here.

The prior completed package is run42068055038 / artifact13436721142, source8249e234. Cloud B's separate strict-XPT package work is not treated as this artifact or as validation of this head. No merge, release or Done transition is part of this Draft.
