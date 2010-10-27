var merge_type_maps_x = 0, merge_type_maps_y = 0;
function merge_type_maps() {
    for (merge_type_maps_x = 0; merge_type_maps_x < 50; ++merge_type_maps_x)
        if ((merge_type_maps_x & 1) == 1)
            ++merge_type_maps_y;
    return [merge_type_maps_x,merge_type_maps_y].join(",");
}
merge_type_maps();

