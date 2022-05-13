export let loadCount = 0;
loadCount++;

export let value = 0;
import {value as importedValue} from "./es6import.js";
value = importedValue + 1;
