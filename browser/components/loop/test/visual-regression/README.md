# How to use the visual regression tool

Note that this is a *work-in-progress*; watch out for *rough edges*.  In
 particular, it can generate a fair number of false positives, most
 visually fairly obvious.  That said, for CSS rule changes that may effect
 multiple views, it can be extremely handy for detecting subtle regressions.

It works by taking a snapshot of each view in the ui-showcase, and then
 comparing it to a previous version of the same snapshot, generating an image
 which shows differences between the two.

## Requirements:

```bash
easy_install pip
pip install selenium
pip install Pillow
```

## File hierarchy

./loop/test/visual-regression
  - README
  - screenshot -- the script that you will run
  - /refs       -- initial screenshots
  - /new        -- screenshots after changes have been made
  - /diff       -- combination of reference screenshots, new and the visual diff between the two

You will need to create those 3 folders.

## Commands

```
screenshot -h      -- provides information
screenshot --refs  -- creates reference file
screenshot --diffs -- creates new screenshots and the diff against the reference files
```

## Typical session, to test a patch for regression against fx-team:

```bash
git checkout fx-team
cd browser/components/loop/test/visual-regression

# create the dirs you'll need
mkdir refs new diff
# generate the reference images from the ui-showcase
# take a break; this takes at least 10 mins on a 2015 i7 Retina mac
./screenshot --refs

# create a branch and apply the patch you want to check
git checkout -b bugzilla-bug-1234
git bz apply 1234

# generate the new images, and then diff them.  take another long break.
./screenshot --diffs

cd diff

# this is a mac command, other OSes will vary in how to view all the images
open -a Preview *

# diffs have the reference image on left, the diff in the middle, and the
# new image on right, all joined into a single image so you can see them
# together.  look for images that have red in the diff section to find potential
# regressions
```

## Note

Because Selenium launches a separate browser window you can make changes to the
CSS or markup without having to worry that it will affect the screenshots. You
can generate the diffs after you have finished your changes before submitting
the patch.
