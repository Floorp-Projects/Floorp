import { usize } from "./diplomat-runtime"

/**

 * Thin wrapper around a vector that maps visual indices to source indices

 * `map[visualIndex] = sourceIndex`

 * Produced by `reorder_visual()` on {@link ICU4XBidi `ICU4XBidi`}.
 */
export class ICU4XReorderedIndexMap {

  /**

   * Get this as a slice/array of indices
   */
  as_slice(): Uint32Array;

  /**

   * The length of this map
   */
  len(): usize;

  /**

   * Get element at `index`. Returns 0 when out of bounds (note that 0 is also a valid in-bounds value, please use `len()` to avoid out-of-bounds)
   */
  get(index: usize): usize;
}
