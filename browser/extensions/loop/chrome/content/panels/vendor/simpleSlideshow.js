"use strict"; // This is derived from PIOTR F's code,
// currently available at https://github.com/piotrf/simple-react-slideshow

// Simple React Slideshow Example
//
// Original Author: PIOTR F.
// License: MIT
//
// Copyright (c) 2015 Piotr
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

var loop = loop || {};
loop.SimpleSlideshow = function () {
  "use strict";

  // App state
  var state = { 
    currentSlide: 0, 
    data: [] };


  // State transitions
  var actions = { 
    toggleNext: function toggleNext() {
      var current = state.currentSlide;
      var next = current + 1;
      if (next < state.data.length) {
        state.currentSlide = next;}

      render();}, 

    togglePrev: function togglePrev() {
      var current = state.currentSlide;
      var prev = current - 1;
      if (prev >= 0) {
        state.currentSlide = prev;}

      render();}, 

    toggleSlide: function toggleSlide(id) {
      var index = state.data.map(function (el) {
        return (
          el.id);});


      var currentIndex = index.indexOf(id);
      state.currentSlide = currentIndex;
      render();} };



  var Slideshow = React.createClass({ displayName: "Slideshow", 
    propTypes: { 
      data: React.PropTypes.array.isRequired }, 

    render: function render() {
      return (
        React.createElement("div", { className: "slideshow" }, 
        React.createElement(Slides, { data: this.props.data }), 
        React.createElement("div", { className: "control-panel" }, 
        React.createElement(Controls, null))));} });






  var Slides = React.createClass({ displayName: "Slides", 
    propTypes: { 
      data: React.PropTypes.array.isRequired }, 

    render: function render() {
      var slidesNodes = this.props.data.map(function (slideNode, index) {
        var isActive = state.currentSlide === index;
        return (
          React.createElement(Slide, { active: isActive, 
            imageClass: slideNode.imageClass, 
            indexClass: slideNode.id, 
            text: slideNode.text, 
            title: slideNode.title }));});


      return (
        React.createElement("div", { className: "slides" }, 
        slidesNodes));} });





  var Slide = React.createClass({ displayName: "Slide", 
    propTypes: { 
      active: React.PropTypes.bool.isRequired, 
      imageClass: React.PropTypes.string.isRequired, 
      indexClass: React.PropTypes.string.isRequired, 
      text: React.PropTypes.string.isRequired, 
      title: React.PropTypes.string.isRequired }, 

    render: function render() {
      var classes = classNames({ 
        "slide": true, 
        "slide--active": this.props.active });

      return (

        React.createElement("div", { className: classes }, 
        React.createElement("div", { className: this.props.indexClass }, 
        React.createElement("div", { className: "slide-layout" }, 
        React.createElement("img", { className: this.props.imageClass }), 
        React.createElement("h2", null, this.props.title), 
        React.createElement("div", { className: "slide-text" }, this.props.text)))));} });







  var Controls = React.createClass({ displayName: "Controls", 
    togglePrev: function togglePrev() {
      actions.togglePrev();}, 

    toggleNext: function toggleNext() {
      actions.toggleNext();}, 

    render: function render() {
      var showPrev, showNext;
      var current = state.currentSlide;
      var last = state.data.length;
      if (current > 0) {
        showPrev = React.createElement("div", { className: "toggle toggle-prev", onClick: this.togglePrev });}

      if (current < last - 1) {
        showNext = React.createElement("div", { className: "toggle toggle-next", onClick: this.toggleNext });}

      return (
        React.createElement("div", { className: "controls" }, 
        showPrev, 
        showNext));} });





  var EmptyMessage = React.createClass({ displayName: "EmptyMessage", 
    render: function render() {
      return (
        React.createElement("div", { className: "empty-message" }, "No Data"));} });




  function render(renderTo) {
    var hasData = state.data.length > 0;
    var component;
    if (hasData) {
      component = React.createElement(Slideshow, { data: state.data });} else 

    {
      component = React.createElement(EmptyMessage, null);}

    ReactDOM.render(
    component, 
    document.querySelector(renderTo ? renderTo : "#main"));}



  function init(renderTo, data) {
    state.data = data;
    render(renderTo);}


  return { 
    init: init };}();
