
export default {
	input: 'src/index.js',
	output: {
		file: 'bundle.js',
		format: 'iife',
    sourcemap: true,
    sourcemapIgnoreList: (relativeSourcePath) => {
      // Adding original-1.js and original-3.js to the ignore list should cause these files to
      // be ignored by the devtools and their debugger statements should not be hit.
      return ['original-1', 'original-3'].some(fileName => relativeSourcePath.includes(fileName));
    },
	},
};