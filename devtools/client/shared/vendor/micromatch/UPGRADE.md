# STEPS TO UPGRADE `MICROMATCH`

# Go to the `micromatch` vendor folder

1. cd devtools/client/shared/vendor/micromatch/ folder

# Install the node package (Remeber to bump the version (to the latest) in the package.json)

2. npm install

# Generate the bundle (creates a `micromatch.js` file)

3. npm run webpack

# Cleanup all the folders and files

4. rm -rf node_modules
