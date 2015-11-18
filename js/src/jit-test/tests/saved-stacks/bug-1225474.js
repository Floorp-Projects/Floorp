// setSavedStacksRNGState shouldn't crash regardless of the seed value passed.

setSavedStacksRNGState(0);
setSavedStacksRNGState({});
setSavedStacksRNGState(false);
setSavedStacksRNGState(NaN);
