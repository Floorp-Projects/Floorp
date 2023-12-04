export default {
  entry: {
    index: [
      './src/ts/app.ts',
      './src/scss/styles.scss',
    ],
  },
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: 'ts-loader',
        exclude: /node_modules/,
      },
      {
        test: /\.(scss)$/,
        use: [
          {
            loader: 'style-loader',
          },
          {
            loader: 'css-loader'
          },
          {
            loader: 'postcss-loader',
            options: {
              postcssOptions: {
                plugins: () => [
                  require('autoprefixer')
                ]
              }
            }
          },
          {
            loader: 'sass-loader'
          }
        ]
      }
    ]
  },
  resolve: {
    extensions: ['.ts', '.js'],
    fallback: {
      "fs": false,
    },
  },
  mode: "production",
  // mode: "development",
  output: {
    filename: 'bundle.js',
    path: new URL('dist', import.meta.url).pathname,
  },
  devServer: {
    static: '.',
    port: 8080,
    hot: true, 
    headers: {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, PATCH, OPTIONS",
      "Access-Control-Allow-Headers": "X-Requested-With, content-type, Authorization"
    }
  },
  experiments: {
    topLevelAwait: true,
  },
  optimization: {
    minimize: false
  },
};
