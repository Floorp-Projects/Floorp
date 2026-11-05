# KAN-24 dependent Draft and full-product follow-up

Canonical Draft2950 remains unchanged at69d306b11c9e52da9e6270870740d49b1d561659. Draft3046 is a dependent supplement against an unmodified same-repository mirror of that head; it is not an alternative fix. The author fork is not updated.

The dependent source uses locked Runtime158. Latest-main integration493d2974e00e6902d86585d329eea1f9040ff791 uses main29a3eb23145da90706d8a94441373278521cc737 and locked Runtime159, plus exactly the canonical eight-file patch and dependent four-file supplement/tests. No Runtime lock or workflow change.

Package runs42093687541(baseline) and42094061748(integration) use the existing package.yml unsigned stable-v2-validation route. This assembles the complete production frontend/application/package using authenticated locked prebuilt native Runtime; it is not a fresh Gecko compilation or a public signed-release qualification. Standard CI runs42093984715(dependent158) and42094068516(integration159) are separate.

UI scripts accept only authenticated unmodified full products. Baseline creates a natural mismatch by real synthetic Hub/native controls; candidate copies the normally quit profile and checks cold startup on about:blank before navigating to Hub. Browser profiles, migration-seed, products, credentials and signed download URLs are excluded from publication.

Original macOS Photon remains outstanding and is not accessed while the Mac owner is active. Linux22 denominator and original assigned7matrix remain unchanged. KAN7 GPU conditions are not exercised by these tests. No merge, release, public distribution or Done transition.

The unchanged existing package workflow relaxes empty auxiliary xpt/framework archive checks for artifact packaging (package-workflow-scope.json). This is explicitly not strict final-release/helper/XPT qualification. No new validation relaxation or native helper substitution is introduced by KAN24.
