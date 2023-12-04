import { ICU4XDataProvider } from 'icu4x';
import * as fdf from './fixed-decimal';
import * as dtf from './date-time';
import * as seg from './segmenter';

import 'bootstrap/js/dist/tab';
import 'bootstrap/js/dist/dropdown';
import 'bootstrap/js/dist/collapse';

(async function init() {
    const dataProvider = ICU4XDataProvider.create_compiled();
    fdf.setup(dataProvider);
    dtf.setup(dataProvider);
    seg.setup(dataProvider);
    (document.querySelector("#bigspinner") as HTMLElement).style.display = "none";
})()