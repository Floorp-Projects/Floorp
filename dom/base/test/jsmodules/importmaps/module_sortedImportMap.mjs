/* eslint-disable import/no-unresolved */
import { x } from "scope1/scope2/module_simpleExport.mjs";
import { x as y } from "scope1/scope2/scope3/scope4/module_simpleExport.mjs";

sorted_result = x;
sorted_result2 = y;
