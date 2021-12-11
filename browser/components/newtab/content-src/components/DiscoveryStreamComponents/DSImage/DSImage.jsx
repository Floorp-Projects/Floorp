/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

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
      } else if (!this.state.nonOptimizedImageFailed) {
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
        // Remove the img element if both sources fail. Render a placeholder instead.
        img = <div className="broken-image" />;
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
