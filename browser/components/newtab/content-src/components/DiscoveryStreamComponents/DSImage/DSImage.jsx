import {cache} from "./cache";
import React from "react";
import ReactDOM from "react-dom";

export class DSImage extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onOptimizedImageError = this.onOptimizedImageError.bind(this);

    this.state = {
      isSeen: false,
      optimizedImageFailed: false,
    };
  }

  onSeen(entries) {
    if (this.state) {
      if (entries.some(entry => entry.isIntersecting)) {
        if (this.props.optimize) {
          this.setState({
            containerWidth: ReactDOM.findDOMNode(this).clientWidth,
          });
        }

        this.setState({
          isSeen: true,
        });

        // Stop observing since element has been seen
        this.observer.unobserve(ReactDOM.findDOMNode(this));
      }
    }
  }

  reformatImageURL(url, width) {
    // Change the image URL to request a size tailored for the parent container width
    // Also: force JPEG, quality 60, no upscaling, no EXIF data
    // Uses Thumbor: https://thumbor.readthedocs.io/en/latest/usage.html
    return `https://img-getpocket.cdn.mozilla.net/${width}x0/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(url)}`;
  }

  componentDidMount() {
    this.observer = new IntersectionObserver(this.onSeen.bind(this));
    this.observer.observe(ReactDOM.findDOMNode(this));
  }

  componentWillUnmount() {
    // Remove observer on unmount
    if (this.observer) {
      this.observer.unobserve(ReactDOM.findDOMNode(this));
    }
  }

  render() {
    const classNames = `ds-image${this.props.extraClassNames ? ` ${this.props.extraClassNames}` : ``}`;

    let img;

    if (this.state && this.state.isSeen) {
      if (this.props.optimize && this.props.rawSource && !this.state.optimizedImageFailed) {
        let source;
        let source2x;

        if (this.state && this.state.containerWidth) {
          let baseSource = this.props.rawSource;

          source = this.reformatImageURL(
            baseSource,
            cache.query(baseSource, this.state.containerWidth, `1x`)
          );

          source2x = this.reformatImageURL(
            baseSource,
            cache.query(baseSource, this.state.containerWidth * 2, `2x`)
          );

          img = (<img onError={this.onOptimizedImageError} src={source} srcSet={`${source2x} 2x`} />);
        }
      } else {
        img = (<img src={this.props.source} />);
      }
    }

    return (
      <picture className={classNames}>{img}</picture>
    );
  }

  onOptimizedImageError() {
    // This will trigger a re-render and the unoptimized 450px image will be used as a fallback
    this.setState({
      optimizedImageFailed: true,
    });
  }
}

DSImage.defaultProps = {
  source: null, // The current source style from Pocket API (always 450px)
  rawSource: null, // Unadulterated image URL to filter through Thumbor
  extraClassNames: null, // Additional classnames to append to component
  optimize: true, // Measure parent container to request exact sizes
};
