/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

const PLACEHOLDER_IMAGE_DATA_ARRAY = [
  {
    rotation: "0deg",
    offsetx: "20px",
    offsety: "8px",
    scale: "45%",
  },
  {
    rotation: "54deg",
    offsetx: "-26px",
    offsety: "62px",
    scale: "55%",
  },
  {
    rotation: "-30deg",
    offsetx: "78px",
    offsety: "30px",
    scale: "68%",
  },
  {
    rotation: "-22deg",
    offsetx: "0",
    offsety: "92px",
    scale: "60%",
  },
  {
    rotation: "-65deg",
    offsetx: "66px",
    offsety: "28px",
    scale: "60%",
  },
  {
    rotation: "22deg",
    offsetx: "-35px",
    offsety: "62px",
    scale: "52%",
  },
  {
    rotation: "-25deg",
    offsetx: "86px",
    offsety: "-15px",
    scale: "68%",
  },
];

const PLACEHOLDER_IMAGE_COLORS_ARRAY = "#0090ED #FF4F5F #2AC3A2 #FF7139 #A172FF #FFA437 #FF2A8A".split(
  " "
);

function generateIndex({ keyCode, max }) {
  if (!keyCode) {
    // Just grab a random index if we cannot generate an index from a key.
    return Math.floor(Math.random() * max);
  }

  const hashStr = str => {
    let hash = 0;
    for (let i = 0; i < str.length; i++) {
      let charCode = str.charCodeAt(i);
      hash += charCode;
    }
    return hash;
  };

  const hash = hashStr(keyCode);
  return hash % max;
}

export function PlaceholderImage({ urlKey, titleKey }) {
  const dataIndex = generateIndex({
    keyCode: urlKey,
    max: PLACEHOLDER_IMAGE_DATA_ARRAY.length,
  });
  const colorIndex = generateIndex({
    keyCode: titleKey,
    max: PLACEHOLDER_IMAGE_COLORS_ARRAY.length,
  });
  const { rotation, offsetx, offsety, scale } = PLACEHOLDER_IMAGE_DATA_ARRAY[
    dataIndex
  ];
  const color = PLACEHOLDER_IMAGE_COLORS_ARRAY[colorIndex];
  const style = {
    "--placeholderBackgroundColor": color,
    "--placeholderBackgroundRotation": rotation,
    "--placeholderBackgroundOffsetx": offsetx,
    "--placeholderBackgroundOffsety": offsety,
    "--placeholderBackgroundScale": scale,
  };

  return <div style={style} className="placeholder-image" />;
}

export class DSImage extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onOptimizedImageError = this.onOptimizedImageError.bind(this);
    this.onNonOptimizedImageError = this.onNonOptimizedImageError.bind(this);
    this.onLoad = this.onLoad.bind(this);

    this.state = {
      isLoaded: false,
      optimizedImageFailed: false,
      useTransition: false,
    };
  }

  onIdleCallback() {
    if (!this.state.isLoaded) {
      this.setState({
        useTransition: true,
      });
    }
  }

  reformatImageURL(url, width, height) {
    // Change the image URL to request a size tailored for the parent container width
    // Also: force JPEG, quality 60, no upscaling, no EXIF data
    // Uses Thumbor: https://thumbor.readthedocs.io/en/latest/usage.html
    return `https://img-getpocket.cdn.mozilla.net/${width}x${height}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(
      url
    )}`;
  }

  componentDidMount() {
    this.idleCallbackId = this.props.windowObj.requestIdleCallback(
      this.onIdleCallback.bind(this)
    );
  }

  componentWillUnmount() {
    if (this.idleCallbackId) {
      this.props.windowObj.cancelIdleCallback(this.idleCallbackId);
    }
  }

  render() {
    let classNames = `ds-image
      ${this.props.extraClassNames ? ` ${this.props.extraClassNames}` : ``}
      ${this.state && this.state.useTransition ? ` use-transition` : ``}
      ${this.state && this.state.isLoaded ? ` loaded` : ``}
    `;

    let img;

    if (this.state) {
      if (
        this.props.optimize &&
        this.props.rawSource &&
        !this.state.optimizedImageFailed
      ) {
        let baseSource = this.props.rawSource;

        let sizeRules = [];
        let srcSetRules = [];

        for (let rule of this.props.sizes) {
          let { mediaMatcher, width, height } = rule;
          let sizeRule = `${mediaMatcher} ${width}px`;
          sizeRules.push(sizeRule);
          let srcSetRule = `${this.reformatImageURL(
            baseSource,
            width,
            height
          )} ${width}w`;
          let srcSetRule2x = `${this.reformatImageURL(
            baseSource,
            width * 2,
            height * 2
          )} ${width * 2}w`;
          srcSetRules.push(srcSetRule);
          srcSetRules.push(srcSetRule2x);
        }

        if (this.props.sizes.length) {
          // We have to supply a fallback in the very unlikely event that none of
          // the media queries match. The smallest dimension was chosen arbitrarily.
          sizeRules.push(
            `${this.props.sizes[this.props.sizes.length - 1].width}px`
          );
        }

        img = (
          <img
            loading="lazy"
            alt={this.props.alt_text}
            crossOrigin="anonymous"
            onLoad={this.onLoad}
            onError={this.onOptimizedImageError}
            sizes={sizeRules.join(",")}
            src={baseSource}
            srcSet={srcSetRules.join(",")}
          />
        );
      } else if (this.props.source && !this.state.nonOptimizedImageFailed) {
        img = (
          <img
            loading="lazy"
            alt={this.props.alt_text}
            crossOrigin="anonymous"
            onLoad={this.onLoad}
            onError={this.onNonOptimizedImageError}
            src={this.props.source}
          />
        );
      } else {
        // We consider a failed to load img or source without an image as loaded.
        classNames = `${classNames} loaded`;
        // Remove the img element if both sources fail. Render a placeholder instead.
        // This only happens if the sources are invalid or all attempts to load it failed.
        img = (
          <PlaceholderImage
            urlKey={this.props.url}
            titleKey={this.props.title}
          />
        );
      }
    }

    return <picture className={classNames}>{img}</picture>;
  }

  onOptimizedImageError() {
    // This will trigger a re-render and the unoptimized 450px image will be used as a fallback
    this.setState({
      optimizedImageFailed: true,
    });
  }

  onNonOptimizedImageError() {
    this.setState({
      nonOptimizedImageFailed: true,
    });
  }

  onLoad() {
    this.setState({
      isLoaded: true,
    });
  }
}

DSImage.defaultProps = {
  source: null, // The current source style from Pocket API (always 450px)
  rawSource: null, // Unadulterated image URL to filter through Thumbor
  extraClassNames: null, // Additional classnames to append to component
  optimize: true, // Measure parent container to request exact sizes
  alt_text: null,
  windowObj: window, // Added to support unit tests
  sizes: [],
};
