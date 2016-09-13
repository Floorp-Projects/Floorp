function waitForIterationChange(animation) {
  var initialIteration = animation.effect.getComputedTiming().currentIteration;
  return new Promise(resolve => {
    window.requestAnimationFrame(handleFrame = () => {
      if (animation.effect.getComputedTiming().currentIteration !=
            initialIteration) {
        resolve();
      } else {
        window.requestAnimationFrame(handleFrame);
      }
    });
  });
}
